#pragma once

#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"

namespace pdf2docx {

struct IrJsonOptions {
  bool include_summary = true;
  bool include_image_quad = true;
};

Status WriteIrToJson(const ir::Document& document,
                     const std::string& output_json,
                     const IrJsonOptions& options = {});

}  // namespace pdf2docx
