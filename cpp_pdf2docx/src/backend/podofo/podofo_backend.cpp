#include "backend/podofo/podofo_backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
#include <utility>

#if PDF2DOCX_HAS_PODOFO
#include <podofo/podofo.h>
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

ir::Rect TransformUnitRect(const PoDoFo::Matrix& matrix) {
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

  return ir::Rect{
      .x = min_x,
      .y = min_y,
      .width = std::max(0.0, max_x - min_x),
      .height = std::max(0.0, max_y - min_y),
  };
}

bool TryInferImageFormat(const PoDoFo::PdfFilterList& filters,
                         std::string* out_extension,
                         std::string* out_mime_type) {
  if (out_extension == nullptr || out_mime_type == nullptr) {
    return false;
  }

  for (auto filter : filters) {
    switch (filter) {
      case PoDoFo::PdfFilterType::DCTDecode:
        *out_extension = "jpg";
        *out_mime_type = "image/jpeg";
        return true;
      case PoDoFo::PdfFilterType::JPXDecode:
        *out_extension = "jp2";
        *out_mime_type = "image/jp2";
        return true;
      default:
        break;
    }
  }
  return false;
}

std::optional<ExtractedImageBinary> TryExtractImageBinary(const PoDoFo::PdfImage& image) {
  const PoDoFo::PdfObjectStream* stream = image.GetObject().GetStream();
  if (stream == nullptr) {
    return std::nullopt;
  }

  std::string extension;
  std::string mime_type;
  const auto& filters = const_cast<PoDoFo::PdfObjectStream*>(stream)->GetFilters();
  if (!TryInferImageFormat(filters, &extension, &mime_type)) {
    return std::nullopt;
  }

  PoDoFo::charbuff buffer = stream->GetCopySafe();
  if (buffer.empty()) {
    return std::nullopt;
  }

  ExtractedImageBinary result;
  result.extension = std::move(extension);
  result.mime_type = std::move(mime_type);
  result.data.assign(buffer.begin(), buffer.end());
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
                                  ir::Document* out_document) const {
  if (out_document == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "out_document is null");
  }

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

          auto extracted = TryExtractImageBinary(*image);
          if (!extracted.has_value()) {
            continue;
          }

          const PoDoFo::Matrix placement_matrix = current_matrix * xobject->GetMatrix();
          ir::Rect image_rect = TransformUnitRect(placement_matrix);
          if (image_rect.width <= 0.0 || image_rect.height <= 0.0) {
            image_rect.width = std::max(1.0, static_cast<double>(image->GetWidth()));
            image_rect.height = std::max(1.0, static_cast<double>(image->GetHeight()));
          }

          ir::ImageBlock image_block;
          image_block.id = "p" + std::to_string(page_index + 1) + "_img" + std::to_string(ir_page.images.size() + 1);
          image_block.mime_type = std::move(extracted->mime_type);
          image_block.extension = std::move(extracted->extension);
          image_block.x = image_rect.x;
          image_block.y = image_rect.y;
          image_block.width = image_rect.width;
          image_block.height = image_rect.height;
          image_block.data = std::move(extracted->data);
          ir_page.images.push_back(std::move(image_block));
        }
      }

      out_document->pages.push_back(std::move(ir_page));
    }
  } catch (const std::exception& exception) {
    return Status::Error(ErrorCode::kPdfParseFailed, std::string("podofo extraction failed: ") + exception.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "podofo extraction failed with unknown error");
  }

  return Status::Ok();
#else
  (void)pdf_path;
  (void)options;
  out_document->pages.clear();
  return Status::Error(ErrorCode::kUnsupportedFeature, "PoDoFo is not available in this build");
#endif
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
