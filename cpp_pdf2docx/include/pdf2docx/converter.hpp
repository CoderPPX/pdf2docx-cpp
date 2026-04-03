#pragma once

#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"
#include "pdf2docx/types.hpp"

namespace pdf2docx {

class Converter {
 public:
  explicit Converter(BackendKind backend = BackendKind::kPoDoFo);

  Status ConvertFile(const std::string& input_pdf,
                     const std::string& output_docx,
                     const ConvertOptions& options,
                     ConvertStats* stats) const;

  Status ExtractIrFromFile(const std::string& input_pdf,
                           const ConvertOptions& options,
                           ir::Document* document) const;

 private:
  BackendKind backend_;
};

}  // namespace pdf2docx
