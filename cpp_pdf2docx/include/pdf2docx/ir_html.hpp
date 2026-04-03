#pragma once

#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"

namespace pdf2docx {

struct IrHtmlOptions {
  double scale = 1.25;
  bool show_boxes = true;
};

Status WriteIrToHtml(const ir::Document& document,
                     const std::string& output_html,
                     const IrHtmlOptions& options = {});

}  // namespace pdf2docx
