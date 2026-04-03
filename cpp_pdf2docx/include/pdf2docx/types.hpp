#pragma once

#include <cstdint>
#include <string>

namespace pdf2docx {

enum class BackendKind {
  kPoDoFo,
  kPoppler,
};

struct ImageExtractionStats {
  uint32_t extracted_count = 0;
  uint32_t skipped_count = 0;
  uint32_t skipped_unsupported_filter_count = 0;
  uint32_t skipped_empty_stream_count = 0;
  uint32_t skipped_decode_failed_count = 0;
  uint32_t warning_count = 0;
};

struct ConvertOptions {
  bool preserve_page_breaks = true;
  bool extract_images = true;
  bool detect_tables = false;
  bool enable_font_fallback = true;
  bool docx_use_anchored_images = false;
  uint32_t max_threads = 0;
};

struct ConvertStats {
  uint32_t page_count = 0;
  uint32_t image_count = 0;
  uint32_t warning_count = 0;
  uint32_t extracted_image_count = 0;
  uint32_t skipped_image_count = 0;
  uint32_t skipped_unsupported_filter_count = 0;
  uint32_t skipped_empty_stream_count = 0;
  uint32_t skipped_decode_failed_count = 0;
  uint32_t font_probe_count = 0;
  uint32_t backend_warning_count = 0;
  double elapsed_ms = 0.0;
  std::string backend;
  std::string xml_backend;
};

}  // namespace pdf2docx
