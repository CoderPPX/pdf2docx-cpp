#pragma once

#include <cstdint>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/status.hpp"

namespace pdf2docx::pipeline {

struct PipelineStats {
  uint32_t page_count = 0;
  uint32_t reordered_page_count = 0;
  uint32_t reordered_span_count = 0;
};

class Pipeline {
 public:
  Status Execute(ir::Document* document, PipelineStats* stats = nullptr) const;
};

}  // namespace pdf2docx::pipeline
