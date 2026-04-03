#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "pdf2docx/converter.hpp"
#include "pdf2docx/ir_html.hpp"

int main() {
  const std::filesystem::path fixture_pdf = PDF2DOCX_TEST_PDF_PATH;
  if (!std::filesystem::exists(fixture_pdf)) {
    return EXIT_FAILURE;
  }

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ir::Document document;
  pdf2docx::Status status = converter.ExtractIrFromFile(fixture_pdf.string(), options, &document);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  if (document.pages.empty()) {
    return EXIT_FAILURE;
  }

  size_t image_count = 0;
  for (const auto& page : document.pages) {
    image_count += page.images.size();
  }
  if (image_count == 0) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path output_html =
      std::filesystem::temp_directory_path() / "pdf2docx_fixture_ir_preview.html";
  status = pdf2docx::WriteIrToHtml(document, output_html.string());
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  std::ifstream html_stream(output_html, std::ios::in);
  if (!html_stream.is_open()) {
    return EXIT_FAILURE;
  }
  std::string html((std::istreambuf_iterator<char>(html_stream)), std::istreambuf_iterator<char>());
  if (html.find("Page 1") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("class=\"page-canvas\"") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (html.find("class=\"image") == std::string::npos) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
