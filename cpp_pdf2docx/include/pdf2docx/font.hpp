#pragma once

#include <string>

#include "pdf2docx/status.hpp"

namespace pdf2docx {

struct FreeTypeProbeInfo {
  bool available = false;
  int major_version = 0;
  int minor_version = 0;
  int patch_version = 0;
  std::string version;
  bool has_truetype_module = false;
  bool has_cff_module = false;
};

Status ProbeFreeType(FreeTypeProbeInfo* info = nullptr);

}  // namespace pdf2docx
