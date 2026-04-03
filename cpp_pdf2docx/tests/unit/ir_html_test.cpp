#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "pdf2docx/ir.hpp"
#include "pdf2docx/ir_html.hpp"

int main() {
  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595;
  page.height_pt = 842;
  pdf2docx::ir::TextSpan span;
  span.text = "hello <ir> & html";
  span.x = 10;
  span.y = 10;
  span.length = 18;
  span.has_bbox = true;
  span.bbox = pdf2docx::ir::Rect{10, 10, 60, 12};
  page.spans.push_back(span);
  document.pages.push_back(page);

  const std::filesystem::path output = std::filesystem::temp_directory_path() / "pdf2docx_ir_html_test.html";
  pdf2docx::IrHtmlOptions options;
  options.scale = 1.5;
  if (!pdf2docx::WriteIrToHtml(document, output.string(), options).ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output, std::ios::in);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }

  std::string html((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (html.find("<html>") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("&lt;ir&gt; &amp; html") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("class=\"page-canvas\"") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("position:absolute") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("class=\"span debug\"") == std::string::npos) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
