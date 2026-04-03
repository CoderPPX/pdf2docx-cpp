#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "docx/p0_writer.hpp"
#include "pdf2docx/ir.hpp"
#include "pdf2docx/types.hpp"

int main() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_docx_image_test.docx";

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "image test", .x = 10.0, .y = 20.0, .length = 20.0});
  page.images.push_back(pdf2docx::ir::ImageBlock{
      .id = "img1",
      .mime_type = "image/jpeg",
      .extension = "jpg",
      .x = 30.0,
      .y = 100.0,
      .width = 120.0,
      .height = 80.0,
      .data = {0xFF, 0xD8, 0xFF, 0xD9},
  });
  document.pages.push_back(std::move(page));

  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats).ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output_path, std::ios::binary);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }

  std::string blob((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  if (blob.find("word/media/image1.jpg") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (blob.find("word/_rels/document.xml.rels") == std::string::npos) {
    return EXIT_FAILURE;
  }
#else
  if (blob.find("placeholder") == std::string::npos) {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
