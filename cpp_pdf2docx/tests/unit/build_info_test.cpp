#include <cstdlib>

#include "pdf2docx/build_info.hpp"

int main() {
  if (!pdf2docx::BuildInfo::has_zlib) {
    return EXIT_FAILURE;
  }
  if (!pdf2docx::BuildInfo::has_tinyxml2) {
    return EXIT_FAILURE;
  }
  if (!pdf2docx::BuildInfo::has_minizip) {
    return EXIT_FAILURE;
  }
  if (!pdf2docx::BuildInfo::has_freetype) {
    return EXIT_FAILURE;
  }
  if (!pdf2docx::BuildInfo::has_podofo) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
