#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/ir_html.hpp"

int main() {
  pdf2docx::ir::Document document;
  for (int i = 0; i < 3; ++i) {
    pdf2docx::ir::Page page;
    page.width_pt = 595.0;
    page.height_pt = 842.0;
    page.spans.push_back(pdf2docx::ir::TextSpan{
        .text = "page" + std::to_string(i + 1),
        .x = 10.0,
        .y = 20.0,
        .length = 30.0,
    });
    document.pages.push_back(std::move(page));
  }

  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "pdf2docx_ir_html_only_page_test.html";

  pdf2docx::IrHtmlOptions options;
  options.only_page = 2;
  if (!pdf2docx::WriteIrToHtml(document, output.string(), options).ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output, std::ios::in);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }
  std::string html((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (html.find("Page 2") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("Page 1") != std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("Page 3") != std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("only_page=2") == std::string::npos) {
    return EXIT_FAILURE;
  }

  options.only_page = 5;
  if (pdf2docx::WriteIrToHtml(document, output.string(), options).ok()) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
