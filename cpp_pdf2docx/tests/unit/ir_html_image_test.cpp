#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/ir_html.hpp"

int main() {
  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 200.0;
  page.height_pt = 120.0;

  pdf2docx::ir::ImageBlock image;
  image.id = "img_1";
  image.mime_type = "image/jpeg";
  image.extension = "jpg";
  image.x = 10.0;
  image.y = 20.0;
  image.width = 50.0;
  image.height = 30.0;
  image.data = {0xFF, 0xD8, 0xFF, 0xD9};
  page.images.push_back(std::move(image));

  document.pages.push_back(std::move(page));

  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "pdf2docx_ir_html_image_test.html";
  if (!pdf2docx::WriteIrToHtml(document, output.string()).ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output, std::ios::in);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }

  std::string html((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (html.find("<img class=\"image") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("data:image/jpeg;base64,/9j/2Q==") == std::string::npos) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
