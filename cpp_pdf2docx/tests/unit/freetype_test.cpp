#include <cstdlib>

#include "pdf2docx/build_info.hpp"
#include "pdf2docx/font.hpp"

int main() {
  pdf2docx::FreeTypeProbeInfo info;
  pdf2docx::Status status = pdf2docx::ProbeFreeType(&info);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  if (!pdf2docx::BuildInfo::has_freetype) {
    if (info.available) {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (!info.available) {
    return EXIT_FAILURE;
  }
  if (info.version.empty()) {
    return EXIT_FAILURE;
  }
  if (info.major_version < 2) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
