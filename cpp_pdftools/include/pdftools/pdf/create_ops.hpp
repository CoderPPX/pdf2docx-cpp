#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pdftools/status.hpp"

namespace pdftools::pdf {

enum class ImageScaleMode {
  kFit = 0,
  kOriginal,
};

struct ImagesToPdfRequest {
  std::vector<std::string> image_paths;
  std::string output_pdf;
  bool use_image_size_as_page = false;
  double page_width_pt = 595.0;   // A4 width
  double page_height_pt = 842.0;  // A4 height
  double margin_pt = 0.0;
  ImageScaleMode scale_mode = ImageScaleMode::kFit;
  bool overwrite = false;
};

struct ImagesToPdfResult {
  uint32_t page_count = 0;
  uint32_t skipped_image_count = 0;
};

Status ImagesToPdf(const ImagesToPdfRequest& request, ImagesToPdfResult* result);

}  // namespace pdftools::pdf
