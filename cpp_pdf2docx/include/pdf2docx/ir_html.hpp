#pragma once

#include <cstdint>
#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"

namespace pdf2docx {

struct IrHtmlOptions {
  double scale = 1.25;
  bool show_boxes = true;
  uint32_t only_page = 0;  // 1-based page index, 0 means render all pages.
};

Status WriteIrToHtml(const ir::Document& document,
                     const std::string& output_html,
                     const IrHtmlOptions& options = {});

}  // namespace pdf2docx
