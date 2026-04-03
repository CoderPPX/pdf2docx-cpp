#pragma once

#include <cstdint>
#include <string>

#include "pdftools/status.hpp"

namespace pdftools::convert {

struct PdfToDocxRequest {
  std::string input_pdf;
  std::string output_docx;
  std::string dump_ir_json_path;
  bool extract_images = true;
  bool enable_font_fallback = true;
  bool use_anchored_images = false;
  bool overwrite = false;
};

struct PdfToDocxResult {
  uint32_t page_count = 0;
  uint32_t image_count = 0;
  uint32_t warning_count = 0;
  uint32_t extracted_image_count = 0;
  uint32_t skipped_image_count = 0;
  double elapsed_ms = 0.0;
  std::string backend;
};

Status ConvertPdfToDocx(const PdfToDocxRequest& request, PdfToDocxResult* result);

}  // namespace pdftools::convert

