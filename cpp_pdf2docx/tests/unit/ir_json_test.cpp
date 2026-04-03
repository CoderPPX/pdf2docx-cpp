#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/ir_json.hpp"

int main() {
  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page1;
  page1.width_pt = 595.0;
  page1.height_pt = 842.0;
  page1.spans.push_back(pdf2docx::ir::TextSpan{.text = "page1 text", .x = 10.0, .y = 20.0, .length = 30.0});
  page1.images.push_back(pdf2docx::ir::ImageBlock{
      .id = "img1",
      .mime_type = "image/png",
      .extension = "png",
      .x = 10.0,
      .y = 30.0,
      .width = 40.0,
      .height = 50.0,
      .has_quad = true,
      .quad = pdf2docx::ir::Quad{
          .p0 = {10.0, 30.0},
          .p1 = {50.0, 30.0},
          .p2 = {10.0, 80.0},
          .p3 = {50.0, 80.0},
      },
      .data = {0x89, 'P', 'N', 'G'},
  });
  document.pages.push_back(std::move(page1));

  pdf2docx::ir::Page page2;
  page2.width_pt = 595.0;
  page2.height_pt = 842.0;
  page2.spans.push_back(pdf2docx::ir::TextSpan{.text = "page2 text", .x = 15.0, .y = 25.0, .length = 35.0});
  document.pages.push_back(std::move(page2));

  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "pdf2docx_ir_json_test.json";
  if (!pdf2docx::WriteIrToJson(document, output.string()).ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output, std::ios::in);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }
  std::string json((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (json.find("\"summary\"") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (json.find("\"total_pages\": 2") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (json.find("\"span_count\": 1") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (json.find("\"image_count\": 1") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (json.find("\"quad\"") == std::string::npos) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
