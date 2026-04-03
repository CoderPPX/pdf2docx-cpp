#pragma once

#include "pdf2docx/status.hpp"

namespace pdf2docx::pipeline {

class Pipeline {
 public:
  Status Execute() const;
};

}  // namespace pdf2docx::pipeline
