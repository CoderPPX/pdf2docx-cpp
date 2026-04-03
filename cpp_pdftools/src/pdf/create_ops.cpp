#include "pdftools/pdf/create_ops.hpp"

#include <algorithm>
#include <filesystem>

#include <podofo/podofo.h>

namespace pdftools::pdf {

namespace {

Status PrepareOutputPath(const std::string& output_path, bool overwrite) {
  if (output_path.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output path must not be empty");
  }

  std::filesystem::path out(output_path);
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && !overwrite) {
    return Status::Error(ErrorCode::kAlreadyExists, "output path already exists", output_path);
  }

  auto parent = out.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, "failed to create output directory", parent.string());
    }
  }

  return Status::Ok();
}

}  // namespace

Status ImagesToPdf(const ImagesToPdfRequest& request, ImagesToPdfResult* result) {
  if (request.image_paths.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "at least one input image is required");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_pdf, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }

  try {
    PoDoFo::PdfMemDocument document;
    PoDoFo::PdfPainter painter;
    uint32_t page_count = 0;
    uint32_t skipped_images = 0;

    for (const auto& image_path : request.image_paths) {
      if (image_path.empty()) {
        return Status::Error(ErrorCode::kInvalidArgument, "image path must not be empty");
      }
      try {
        auto image = document.CreateImage();
        image->Load(image_path);

        double page_width = request.page_width_pt;
        double page_height = request.page_height_pt;
        if (request.use_image_size_as_page) {
          page_width = static_cast<double>(image->GetWidth());
          page_height = static_cast<double>(image->GetHeight());
        }
        if (page_width <= 0.0 || page_height <= 0.0) {
          ++skipped_images;
          continue;
        }

        const PoDoFo::Rect page_size(0.0, 0.0, page_width, page_height);
        auto& page = document.GetPages().CreatePage(page_size);

        const double drawable_width = page_width - (request.margin_pt * 2.0);
        const double drawable_height = page_height - (request.margin_pt * 2.0);
        if (drawable_width <= 0.0 || drawable_height <= 0.0) {
          return Status::Error(ErrorCode::kInvalidArgument, "margin is too large for target page");
        }

        double image_width = static_cast<double>(image->GetWidth());
        double image_height = static_cast<double>(image->GetHeight());
        double scale_x = 1.0;
        double scale_y = 1.0;
        if (request.scale_mode == ImageScaleMode::kFit) {
          const double sx = drawable_width / image_width;
          const double sy = drawable_height / image_height;
          const double scale = std::min(sx, sy);
          scale_x = scale;
          scale_y = scale;
        }

        const double drawn_width = image_width * scale_x;
        const double drawn_height = image_height * scale_y;
        const double draw_x = request.margin_pt + (drawable_width - drawn_width) / 2.0;
        const double draw_y = request.margin_pt + (drawable_height - drawn_height) / 2.0;

        painter.SetCanvas(page);
        painter.DrawImage(*image, draw_x, draw_y, scale_x, scale_y);
        painter.FinishDrawing();
        ++page_count;
      } catch (...) {
        ++skipped_images;
      }
    }

    if (page_count == 0) {
      return Status::Error(ErrorCode::kInvalidArgument, "no valid images were decoded");
    }

    document.Save(request.output_pdf);
    result->page_count = page_count;
    result->skipped_image_count = skipped_images;
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kInternalError, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kInternalError, "failed to create PDF from images");
  }
}

}  // namespace pdftools::pdf
