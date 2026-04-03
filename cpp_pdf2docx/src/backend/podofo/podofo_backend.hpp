#pragma once

#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"
#include "pdf2docx/types.hpp"

namespace pdf2docx::backend {

class PoDoFoBackend {
 public:
  PoDoFoBackend() = default;

  Status Open(const std::string& pdf_path) const;
  Status ExtractToIr(const std::string& pdf_path,
                     const ConvertOptions& options,
                     ir::Document* out_document) const;

 private:
  Status ValidatePdfHeader(const std::string& pdf_path) const;
};

}  // namespace pdf2docx::backend
