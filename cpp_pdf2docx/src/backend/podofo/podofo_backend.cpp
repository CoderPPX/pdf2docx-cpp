#include "backend/podofo/podofo_backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
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

double WrapCoordinate(double value, double extent) {
  if (extent <= 0.0) {
    return value;
  }
  double wrapped = std::fmod(value, extent);
  if (wrapped < 0.0) {
    wrapped += extent;
  }
  return wrapped;
}

bool NeedsPageNormalization(const ir::Rect& rect, double page_width, double page_height) {
  if (page_width <= 0.0 || page_height <= 0.0) {
    return false;
  }
  return rect.x < -2.0 * page_width || rect.x > 3.0 * page_width ||
         rect.y < -2.0 * page_height || rect.y > 3.0 * page_height;
}

bool HasFilter(const PoDoFo::PdfFilterList& filters, PoDoFo::PdfFilterType filter_type) {
  return std::find(filters.begin(), filters.end(), filter_type) != filters.end();
}

std::optional<std::tuple<std::string, std::string>> DetectBinaryMime(const PoDoFo::charbuff& buffer) {
  if (buffer.size() >= 3 &&
      static_cast<uint8_t>(buffer[0]) == 0xFF &&
      static_cast<uint8_t>(buffer[1]) == 0xD8 &&
      static_cast<uint8_t>(buffer[2]) == 0xFF) {
    return std::make_tuple("jpg", "image/jpeg");
  }

  if (buffer.size() >= 8 &&
      static_cast<uint8_t>(buffer[0]) == 0x89 &&
      buffer[1] == 'P' &&
      buffer[2] == 'N' &&
      buffer[3] == 'G' &&
      static_cast<uint8_t>(buffer[4]) == 0x0D &&
      static_cast<uint8_t>(buffer[5]) == 0x0A &&
      static_cast<uint8_t>(buffer[6]) == 0x1A &&
      static_cast<uint8_t>(buffer[7]) == 0x0A) {
    return std::make_tuple("png", "image/png");
  }

  if (buffer.size() >= 12 &&
      static_cast<uint8_t>(buffer[0]) == 0x00 &&
      static_cast<uint8_t>(buffer[1]) == 0x00 &&
      static_cast<uint8_t>(buffer[2]) == 0x00 &&
      static_cast<uint8_t>(buffer[3]) == 0x0C &&
      buffer[4] == 'j' &&
      buffer[5] == 'P' &&
      buffer[6] == ' ' &&
      buffer[7] == ' ' &&
      static_cast<uint8_t>(buffer[8]) == 0x0D &&
      static_cast<uint8_t>(buffer[9]) == 0x0A &&
      static_cast<uint8_t>(buffer[10]) == 0x87 &&
      static_cast<uint8_t>(buffer[11]) == 0x0A) {
    return std::make_tuple("jp2", "image/jp2");
  }

  if (buffer.size() >= 6 &&
      buffer[0] == 'G' &&
      buffer[1] == 'I' &&
      buffer[2] == 'F' &&
      buffer[3] == '8' &&
      (buffer[4] == '7' || buffer[4] == '9') &&
      buffer[5] == 'a') {
    return std::make_tuple("gif", "image/gif");
  }

  if (buffer.size() >= 2 &&
      buffer[0] == 'B' &&
      buffer[1] == 'M') {
    return std::make_tuple("bmp", "image/bmp");
  }

  return std::nullopt;
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

std::optional<ExtractedImageBinary> TryExtractDecodedImageAsPng(const PoDoFo::PdfImage& image) {
#if PDF2DOCX_HAS_ZLIB
  try {
    const uint32_t width = image.GetWidth();
    const uint32_t height = image.GetHeight();
    if (width == 0 || height == 0) {
      return std::nullopt;
    }

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t expected_rgba_size = pixel_count * 4;

    std::vector<uint8_t> rgba;
    rgba.reserve(expected_rgba_size);

    bool decoded = false;
    try {
      PoDoFo::charbuff decoded_rgba;
      image.DecodeTo(decoded_rgba, PoDoFo::PdfPixelFormat::RGBA);
      if (decoded_rgba.size() == expected_rgba_size) {
        rgba.assign(decoded_rgba.begin(), decoded_rgba.end());
        decoded = true;
      }
    } catch (...) {
      decoded = false;
    }

    if (!decoded) {
      try {
        PoDoFo::charbuff decoded_rgb;
        image.DecodeTo(decoded_rgb, PoDoFo::PdfPixelFormat::RGB24);
        const size_t expected_rgb_size = pixel_count * 3;
        if (decoded_rgb.size() == expected_rgb_size) {
          rgba.resize(expected_rgba_size);
          for (size_t i = 0, j = 0; i < expected_rgb_size; i += 3, j += 4) {
            rgba[j + 0] = static_cast<uint8_t>(decoded_rgb[i + 0]);
            rgba[j + 1] = static_cast<uint8_t>(decoded_rgb[i + 1]);
            rgba[j + 2] = static_cast<uint8_t>(decoded_rgb[i + 2]);
            rgba[j + 3] = 255;
          }
          decoded = true;
        }
      } catch (...) {
        decoded = false;
      }
    }

    if (!decoded) {
      try {
        PoDoFo::charbuff decoded_gray;
        image.DecodeTo(decoded_gray, PoDoFo::PdfPixelFormat::Grayscale);
        if (decoded_gray.size() == pixel_count) {
          rgba.resize(expected_rgba_size);
          for (size_t i = 0, j = 0; i < pixel_count; ++i, j += 4) {
            const uint8_t g = static_cast<uint8_t>(decoded_gray[i]);
            rgba[j + 0] = g;
            rgba[j + 1] = g;
            rgba[j + 2] = g;
            rgba[j + 3] = 255;
          }
          decoded = true;
        }
      } catch (...) {
        decoded = false;
      }
    }

    if (!decoded || rgba.size() != expected_rgba_size) {
      return std::nullopt;
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
    auto png = TryExtractDecodedImageAsPng(image);
    if (png.has_value()) {
      result.image = std::move(png);
      return result;
    }
    result.failure_reason = ImageExtractFailureReason::DecodeFailed;
    return result;
  }

  // Fallback 1: detect known binary payload by magic bytes.
  PoDoFo::charbuff raw_buffer;
  try {
    raw_buffer = stream->GetCopy(true);
  } catch (...) {
    raw_buffer = stream->GetCopySafe();
  }
  if (!raw_buffer.empty()) {
    auto detected = DetectBinaryMime(raw_buffer);
    if (detected.has_value()) {
      ExtractedImageBinary binary;
      binary.extension = std::get<0>(detected.value());
      binary.mime_type = std::get<1>(detected.value());
      binary.data.assign(raw_buffer.begin(), raw_buffer.end());
      result.image = std::move(binary);
      return result;
    }
  }

  // Fallback for other filter combinations (including color-space specific
  // cases): try decode-to-RGBA and export PNG.
  auto fallback_png = TryExtractDecodedImageAsPng(image);
  if (fallback_png.has_value()) {
    result.image = std::move(fallback_png);
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

          // Image placement is determined by the current graphics state CTM
          // at the Do operator. Unlike form XObjects, image XObjects do not
          // require an additional object matrix multiplication here.
          const PoDoFo::Matrix placement_matrix = current_matrix;
          ImageGeometry geometry = TransformUnitRect(placement_matrix);
          ir::Rect image_rect = geometry.rect;
          if (image_rect.width <= 0.0 || image_rect.height <= 0.0) {
            image_rect.width = std::max(1.0, static_cast<double>(image->GetWidth()));
            image_rect.height = std::max(1.0, static_cast<double>(image->GetHeight()));
            ++local_image_stats.warning_count;
          }

          if (NeedsPageNormalization(image_rect, ir_page.width_pt, ir_page.height_pt)) {
            const double old_x = image_rect.x;
            const double old_y = image_rect.y;
            image_rect.x = WrapCoordinate(image_rect.x, ir_page.width_pt);
            image_rect.y = WrapCoordinate(image_rect.y, ir_page.height_pt);
            image_rect.width = std::min(image_rect.width, std::max(1.0, ir_page.width_pt));
            image_rect.height = std::min(image_rect.height, std::max(1.0, ir_page.height_pt));
            if (image_rect.x + image_rect.width > ir_page.width_pt) {
              image_rect.x = std::max(0.0, ir_page.width_pt - image_rect.width);
            }
            if (image_rect.y + image_rect.height > ir_page.height_pt) {
              image_rect.y = std::max(0.0, ir_page.height_pt - image_rect.height);
            }

            const double dx = image_rect.x - old_x;
            const double dy = image_rect.y - old_y;
            geometry.quad.p0.x += dx;
            geometry.quad.p0.y += dy;
            geometry.quad.p1.x += dx;
            geometry.quad.p1.y += dy;
            geometry.quad.p2.x += dx;
            geometry.quad.p2.y += dy;
            geometry.quad.p3.x += dx;
            geometry.quad.p3.y += dy;
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
