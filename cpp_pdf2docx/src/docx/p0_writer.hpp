#pragma once

#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"
#include "pdf2docx/types.hpp"

namespace pdf2docx::docx {

struct DocxWriteOptions {
  bool use_anchored_images = false;
};

class P0Writer {
 public:
  Status WriteFromIr(const ir::Document& document,
                     const std::string& output_docx,
                     const ConvertStats& stats,
                     const DocxWriteOptions& options = {}) const;
};

}  // namespace pdf2docx::docx
