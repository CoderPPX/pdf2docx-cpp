#pragma once

#include <cstdint>
#include <string>

namespace pdf2docx {

enum class BackendKind {
  kPoDoFo,
  kPoppler,
};

struct ConvertOptions {
  bool preserve_page_breaks = true;
  bool extract_images = true;
  bool detect_tables = false;
  bool enable_font_fallback = true;
  uint32_t max_threads = 0;
};

struct ConvertStats {
  uint32_t page_count = 0;
  uint32_t image_count = 0;
  uint32_t warning_count = 0;
  double elapsed_ms = 0.0;
  std::string backend;
  std::string xml_backend;
};

}  // namespace pdf2docx
