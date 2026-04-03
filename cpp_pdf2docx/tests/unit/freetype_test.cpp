#include <cstdlib>

#include "pdf2docx/build_info.hpp"
#include "pdf2docx/font.hpp"

int main() {
  pdf2docx::Status status = pdf2docx::ProbeFreeType();
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  if (!pdf2docx::BuildInfo::has_freetype) {
    return EXIT_SUCCESS;
  }

  return EXIT_SUCCESS;
}
