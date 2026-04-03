#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pdftools {

struct OperationStats {
  uint32_t page_count = 0;
  uint32_t object_count = 0;
  double elapsed_ms = 0.0;
  std::vector<std::string> warnings;
};

}  // namespace pdftools

