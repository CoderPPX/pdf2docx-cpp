#include "backend/podofo/podofo_backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if PDF2DOCX_HAS_PODOFO
#include <podofo/podofo.h>
#endif

#if PDF2DOCX_HAS_ZLIB
#include <zlib.h>
#endif

namespace pdf2docx::backend {

#if PDF2DOCX_HAS_PODOFO
namespace {

struct ExtractedImageBinary {
  std::string mime_type;
  std::string extension;
  std::vector<uint8_t> data;
};

struct FormContext {
  PoDoFo::Matrix matrix = PoDoFo::Matrix::Identity;
  size_t graphics_stack_size = 0;
};

enum class ImageExtractFailureReason : uint8_t {
  None = 0,
  UnsupportedFilter,
  EmptyStream,
  DecodeFailed,
};

struct ImageGeometry {
  ir::Rect rect{};
  ir::Quad quad{};
};

struct ImageExtractResult {
  std::optional<ExtractedImageBinary> image;
  ImageExtractFailureReason failure_reason = ImageExtractFailureReason::None;
};

bool TryGetOperandAsReal(const PoDoFo::PdfVariantStack& stack, size_t index, double* out_value) {
  if (out_value == nullptr || stack.GetSize() <= index) {
    return false;
  }
  double value = 0.0;
  if (!stack[index].TryGetReal(value)) {
    return false;
  }
  *out_value = value;
  return true;
}

PoDoFo::Matrix MatrixFromCmOperands(const PoDoFo::PdfVariantStack& stack) {
  double a = 1.0;
  double b = 0.0;
  double c = 0.0;
  double d = 1.0;
  double e = 0.0;
  double f = 0.0;
  if (!TryGetOperandAsReal(stack, 5, &a) ||
      !TryGetOperandAsReal(stack, 4, &b) ||
      !TryGetOperandAsReal(stack, 3, &c) ||
      !TryGetOperandAsReal(stack, 2, &d) ||
      !TryGetOperandAsReal(stack, 1, &e) ||
      !TryGetOperandAsReal(stack, 0, &f)) {
    return PoDoFo::Matrix::Identity;
  }
  return PoDoFo::Matrix(a, b, c, d, e, f);
}

std::array<double, 2> TransformPoint(const PoDoFo::Matrix& matrix, double x, double y) {
  return {
      x * matrix[0] + y * matrix[2] + matrix[4],
      x * matrix[1] + y * matrix[3] + matrix[5],
  };
}

ImageGeometry TransformUnitRect(const PoDoFo::Matrix& matrix) {
  const std::array<std::array<double, 2>, 4> corners = {
      TransformPoint(matrix, 0.0, 0.0),
      TransformPoint(matrix, 1.0, 0.0),
      TransformPoint(matrix, 0.0, 1.0),
      TransformPoint(matrix, 1.0, 1.0),
  };

  double min_x = corners[0][0];
  double max_x = corners[0][0];
  double min_y = corners[0][1];
  double max_y = corners[0][1];
  for (const auto& pt : corners) {
    min_x = std::min(min_x, pt[0]);
    max_x = std::max(max_x, pt[0]);
    min_y = std::min(min_y, pt[1]);
    max_y = std::max(max_y, pt[1]);
  }

  ImageGeometry geometry;
  geometry.rect = ir::Rect{
      .x = min_x,
      .y = min_y,
      .width = std::max(0.0, max_x - min_x),
      .height = std::max(0.0, max_y - min_y),
  };
  geometry.quad = ir::Quad{
      .p0 = ir::Point{corners[0][0], corners[0][1]},
      .p1 = ir::Point{corners[1][0], corners[1][1]},
      .p2 = ir::Point{corners[2][0], corners[2][1]},
      .p3 = ir::Point{corners[3][0], corners[3][1]},
  };
  return geometry;
}

bool HasFilter(const PoDoFo::PdfFilterList& filters, PoDoFo::PdfFilterType filter_type) {
  return std::find(filters.begin(), filters.end(), filter_type) != filters.end();
}

#if PDF2DOCX_HAS_ZLIB
void AppendBe32(std::vector<uint8_t>* output, uint32_t value) {
  output->push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  output->push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  output->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  output->push_back(static_cast<uint8_t>(value & 0xFF));
}

void AppendPngChunk(std::vector<uint8_t>* output,
                    const char chunk_type[4],
                    const std::vector<uint8_t>& data) {
  AppendBe32(output, static_cast<uint32_t>(data.size()));
  output->insert(output->end(), chunk_type, chunk_type + 4);
  if (!data.empty()) {
    output->insert(output->end(), data.begin(), data.end());
  }

  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, reinterpret_cast<const Bytef*>(chunk_type), 4);
  if (!data.empty()) {
    crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
  }
  AppendBe32(output, static_cast<uint32_t>(crc));
}

bool EncodePngRgba(const std::vector<uint8_t>& rgba_data,
                   uint32_t width,
                   uint32_t height,
                   std::vector<uint8_t>* png_data) {
  if (png_data == nullptr || width == 0 || height == 0) {
    return false;
  }

  constexpr size_t kBytesPerPixel = 4;
  const size_t expected_size = static_cast<size_t>(width) * static_cast<size_t>(height) * kBytesPerPixel;
  if (rgba_data.size() != expected_size) {
    return false;
  }

  std::vector<uint8_t> raw_scanlines;
  raw_scanlines.reserve((static_cast<size_t>(1) + static_cast<size_t>(width) * kBytesPerPixel) *
                        static_cast<size_t>(height));
  for (uint32_t y = 0; y < height; ++y) {
    raw_scanlines.push_back(0);  // PNG filter type 0 (None)
    const size_t row_offset = static_cast<size_t>(y) * static_cast<size_t>(width) * kBytesPerPixel;
    raw_scanlines.insert(raw_scanlines.end(),
                         rgba_data.begin() + static_cast<std::ptrdiff_t>(row_offset),
                         rgba_data.begin() +
                             static_cast<std::ptrdiff_t>(row_offset + static_cast<size_t>(width) * kBytesPerPixel));
  }

  uLongf compressed_size = compressBound(static_cast<uLong>(raw_scanlines.size()));
  std::vector<uint8_t> compressed_data(compressed_size);
  const int compress_status = compress2(compressed_data.data(),
                                        &compressed_size,
                                        raw_scanlines.data(),
                                        static_cast<uLong>(raw_scanlines.size()),
                                        Z_BEST_SPEED);
  if (compress_status != Z_OK) {
    return false;
  }
  compressed_data.resize(compressed_size);

  png_data->clear();
  png_data->reserve(8 + 25 + compressed_data.size() + 12);
  constexpr uint8_t kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  png_data->insert(png_data->end(), std::begin(kPngSignature), std::end(kPngSignature));

  std::vector<uint8_t> ihdr_data;
  ihdr_data.reserve(13);
  AppendBe32(&ihdr_data, width);
  AppendBe32(&ihdr_data, height);
  ihdr_data.push_back(8);  // bit depth
  ihdr_data.push_back(6);  // color type RGBA
  ihdr_data.push_back(0);  // compression method
  ihdr_data.push_back(0);  // filter method
  ihdr_data.push_back(0);  // interlace method
  AppendPngChunk(png_data, "IHDR", ihdr_data);
  AppendPngChunk(png_data, "IDAT", compressed_data);
  AppendPngChunk(png_data, "IEND", {});
  return true;
}
#endif

std::optional<ExtractedImageBinary> TryExtractFlateImageAsPng(const PoDoFo::PdfImage& image) {
#if PDF2DOCX_HAS_ZLIB
  try {
    const uint32_t width = image.GetWidth();
    const uint32_t height = image.GetHeight();
    if (width == 0 || height == 0) {
      return std::nullopt;
    }

    PoDoFo::charbuff decoded_rgba;
    image.DecodeTo(decoded_rgba, PoDoFo::PdfPixelFormat::RGBA);
    const size_t expected_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (decoded_rgba.size() != expected_size) {
      return std::nullopt;
    }

    std::vector<uint8_t> rgba(decoded_rgba.size());
    for (size_t i = 0; i < decoded_rgba.size(); ++i) {
      rgba[i] = static_cast<uint8_t>(decoded_rgba[i]);
    }

    std::vector<uint8_t> png_data;
    if (!EncodePngRgba(rgba, width, height, &png_data)) {
      return std::nullopt;
    }

    ExtractedImageBinary result;
    result.extension = "png";
    result.mime_type = "image/png";
    result.data = std::move(png_data);
    return result;
  } catch (...) {
    return std::nullopt;
  }
#else
  (void)image;
  return std::nullopt;
#endif
}

ImageExtractResult TryExtractImageBinary(const PoDoFo::PdfImage& image) {
  ImageExtractResult result;
  const PoDoFo::PdfObjectStream* stream = image.GetObject().GetStream();
  if (stream == nullptr) {
    result.failure_reason = ImageExtractFailureReason::EmptyStream;
    return result;
  }

  const auto& filters = const_cast<PoDoFo::PdfObjectStream*>(stream)->GetFilters();
  if (HasFilter(filters, PoDoFo::PdfFilterType::DCTDecode) ||
      HasFilter(filters, PoDoFo::PdfFilterType::JPXDecode)) {
    PoDoFo::charbuff buffer = stream->GetCopySafe();
    if (buffer.empty()) {
      result.failure_reason = ImageExtractFailureReason::EmptyStream;
      return result;
    }

    ExtractedImageBinary binary;
    if (HasFilter(filters, PoDoFo::PdfFilterType::DCTDecode)) {
      binary.extension = "jpg";
      binary.mime_type = "image/jpeg";
    } else {
      binary.extension = "jp2";
      binary.mime_type = "image/jp2";
    }
    binary.data.assign(buffer.begin(), buffer.end());
    result.image = std::move(binary);
    return result;
  }

  if (HasFilter(filters, PoDoFo::PdfFilterType::FlateDecode)) {
    auto png = TryExtractFlateImageAsPng(image);
    if (png.has_value()) {
      result.image = std::move(png);
      return result;
    }
    result.failure_reason = ImageExtractFailureReason::DecodeFailed;
    return result;
  }

  result.failure_reason = ImageExtractFailureReason::UnsupportedFilter;
  return result;
}

}  // namespace
#endif

Status PoDoFoBackend::Open(const std::string& pdf_path) const {
  if (pdf_path.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input pdf path is empty");
  }
  if (!std::filesystem::exists(pdf_path)) {
    return Status::Error(ErrorCode::kIoError, "input pdf does not exist");
  }

  Status header_status = ValidatePdfHeader(pdf_path);
  if (!header_status.ok()) {
    return header_status;
  }

  return Status::Ok();
}

Status PoDoFoBackend::ExtractToIr(const std::string& pdf_path,
                                  const ConvertOptions& options,
                                  ir::Document* out_document,
                                  ImageExtractionStats* image_stats) const {
  if (out_document == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "out_document is null");
  }

  ImageExtractionStats local_image_stats{};
  Status open_status = Open(pdf_path);
  if (!open_status.ok()) {
    return open_status;
  }

#if PDF2DOCX_HAS_PODOFO
  try {
    PoDoFo::PdfMemDocument document;
    document.Load(pdf_path);

    out_document->pages.clear();
    auto& pages = document.GetPages();
    for (unsigned page_index = 0; page_index < pages.GetCount(); ++page_index) {
      auto& page = pages.GetPageAt(page_index);

      ir::Page ir_page;
      auto rect = page.GetRect();
      ir_page.width_pt = rect.Width;
      ir_page.height_pt = rect.Height;

      std::vector<PoDoFo::PdfTextEntry> entries;
      PoDoFo::PdfTextExtractParams params;
      params.Flags = PoDoFo::PdfTextExtractFlags::TokenizeWords |
                     PoDoFo::PdfTextExtractFlags::ComputeBoundingBox;
      page.ExtractTextTo(entries, params);

      for (const auto& entry : entries) {
        if (entry.Text.empty()) {
          continue;
        }

        ir::TextSpan span;
        span.text = entry.Text;
        span.x = entry.X;
        span.y = entry.Y;
        span.length = entry.Length;

        if (entry.BoundingBox.has_value()) {
          const auto& box = entry.BoundingBox.value();
          span.has_bbox = true;
          span.bbox = ir::Rect{box.X, box.Y, box.Width, box.Height};
        }

        ir_page.spans.push_back(std::move(span));
      }

      if (options.extract_images) {
        PoDoFo::PdfContentStreamReader reader(page);
        PoDoFo::PdfContent content;
        PoDoFo::Matrix current_matrix = PoDoFo::Matrix::Identity;
        std::vector<PoDoFo::Matrix> graphics_state_stack;
        std::vector<FormContext> form_context_stack;

        while (reader.TryReadNext(content)) {
          if (content.GetType() == PoDoFo::PdfContentType::Operator) {
            switch (content.GetOperator()) {
              case PoDoFo::PdfOperator::q:
                graphics_state_stack.push_back(current_matrix);
                break;
              case PoDoFo::PdfOperator::Q:
                if (!graphics_state_stack.empty()) {
                  current_matrix = graphics_state_stack.back();
                  graphics_state_stack.pop_back();
                }
                break;
              case PoDoFo::PdfOperator::cm:
                if (content.GetStack().GetSize() == 6) {
                  current_matrix = current_matrix * MatrixFromCmOperands(content.GetStack());
                }
                break;
              default:
                break;
            }
            continue;
          }

          if (content.GetType() == PoDoFo::PdfContentType::BeginFormXObject) {
            if (content.HasErrors()) {
              continue;
            }

            form_context_stack.push_back(FormContext{
                .matrix = current_matrix,
                .graphics_stack_size = graphics_state_stack.size(),
            });

            const auto& xobject = content.GetXObject();
            if (xobject != nullptr) {
              current_matrix = current_matrix * xobject->GetMatrix();
            }
            continue;
          }

          if (content.GetType() == PoDoFo::PdfContentType::EndFormXObject) {
            if (!form_context_stack.empty()) {
              const FormContext context = form_context_stack.back();
              form_context_stack.pop_back();
              current_matrix = context.matrix;
              if (graphics_state_stack.size() > context.graphics_stack_size) {
                graphics_state_stack.resize(context.graphics_stack_size);
              }
            }
            continue;
          }

          if (content.GetType() != PoDoFo::PdfContentType::DoXObject || content.HasErrors()) {
            continue;
          }

          const auto& xobject = content.GetXObject();
          if (xobject == nullptr || xobject->GetType() != PoDoFo::PdfXObjectType::Image) {
            continue;
          }

          auto* image = dynamic_cast<const PoDoFo::PdfImage*>(xobject.get());
          if (image == nullptr) {
            continue;
          }

          ImageExtractResult extracted;
          try {
            extracted = TryExtractImageBinary(*image);
          } catch (...) {
            extracted.failure_reason = ImageExtractFailureReason::DecodeFailed;
          }
          if (!extracted.image.has_value()) {
            ++local_image_stats.skipped_count;
            switch (extracted.failure_reason) {
              case ImageExtractFailureReason::UnsupportedFilter:
                ++local_image_stats.skipped_unsupported_filter_count;
                ++local_image_stats.warning_count;
                break;
              case ImageExtractFailureReason::EmptyStream:
                ++local_image_stats.skipped_empty_stream_count;
                ++local_image_stats.warning_count;
                break;
              case ImageExtractFailureReason::DecodeFailed:
                ++local_image_stats.skipped_decode_failed_count;
                ++local_image_stats.warning_count;
                break;
              default:
                break;
            }
            continue;
          }

          const PoDoFo::Matrix placement_matrix = current_matrix * xobject->GetMatrix();
          ImageGeometry geometry = TransformUnitRect(placement_matrix);
          ir::Rect image_rect = geometry.rect;
          if (image_rect.width <= 0.0 || image_rect.height <= 0.0) {
            image_rect.width = std::max(1.0, static_cast<double>(image->GetWidth()));
            image_rect.height = std::max(1.0, static_cast<double>(image->GetHeight()));
            ++local_image_stats.warning_count;
          }

          ir::ImageBlock image_block;
          image_block.id = "p" + std::to_string(page_index + 1) + "_img" + std::to_string(ir_page.images.size() + 1);
          image_block.mime_type = std::move(extracted.image->mime_type);
          image_block.extension = std::move(extracted.image->extension);
          image_block.x = image_rect.x;
          image_block.y = image_rect.y;
          image_block.width = image_rect.width;
          image_block.height = image_rect.height;
          image_block.has_quad = true;
          image_block.quad = geometry.quad;
          image_block.data = std::move(extracted.image->data);
          ir_page.images.push_back(std::move(image_block));
          ++local_image_stats.extracted_count;
        }
      }

      out_document->pages.push_back(std::move(ir_page));
    }
  } catch (const std::exception& exception) {
    return Status::Error(ErrorCode::kPdfParseFailed, std::string("podofo extraction failed: ") + exception.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "podofo extraction failed with unknown error");
  }
#else
  (void)pdf_path;
  (void)options;
  out_document->pages.clear();
  (void)image_stats;
  return Status::Error(ErrorCode::kUnsupportedFeature, "PoDoFo is not available in this build");
#endif

  if (image_stats != nullptr) {
    *image_stats = local_image_stats;
  }

  return Status::Ok();
}

Status PoDoFoBackend::ValidatePdfHeader(const std::string& pdf_path) const {
  std::ifstream stream(pdf_path, std::ios::binary);
  if (!stream.is_open()) {
    return Status::Error(ErrorCode::kIoError, "failed to open input pdf");
  }

  char header[5] = {0, 0, 0, 0, 0};
  stream.read(header, sizeof(header));
  if (stream.gcount() < 5) {
    return Status::Error(ErrorCode::kInvalidArgument, "input file is too small to be a pdf");
  }

  if (std::string(header, sizeof(header)) != "%PDF-") {
    return Status::Error(ErrorCode::kInvalidArgument, "input file does not look like a pdf");
  }

  return Status::Ok();
}

}  // namespace pdf2docx::backend
