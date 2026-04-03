#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "docx/p0_writer.hpp"
#include "pdf2docx/build_info.hpp"
#include "pdf2docx/ir.hpp"
#include "pdf2docx/types.hpp"

int main() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_writer_test.docx";

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";
  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "writer test span", .x = 10.0, .y = 20.0, .length = 20.0});
  document.pages.push_back(page);

  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats).ok()) {
    return EXIT_FAILURE;
  }

  if (!std::filesystem::exists(output_path)) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output_path, std::ios::binary);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }

  char first = '\0';
  char second = '\0';
  stream.read(&first, 1);
  stream.read(&second, 1);

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  if (!(first == 'P' && second == 'K')) {
    return EXIT_FAILURE;
  }
#else
  if (first != 'p') {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
