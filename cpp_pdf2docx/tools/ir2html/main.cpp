#include <iostream>
#include <stdexcept>
#include <string>

#include "pdf2docx/converter.hpp"
#include "pdf2docx/ir_html.hpp"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: ir2html <input.pdf> <output.html> [--scale 1.25] [--hide-boxes]\n";
    return 1;
  }

  const std::string input_pdf = argv[1];
  const std::string output_html = argv[2];
  pdf2docx::IrHtmlOptions html_options;

  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--scale") {
      if (i + 1 >= argc) {
        std::cerr << "--scale requires a numeric value\n";
        return 1;
      }
      try {
        html_options.scale = std::stod(argv[++i]);
      } catch (const std::exception&) {
        std::cerr << "invalid --scale value\n";
        return 1;
      }
      continue;
    }
    if (arg == "--hide-boxes") {
      html_options.show_boxes = false;
      continue;
    }
    std::cerr << "Unknown argument: " << arg << "\n";
    return 1;
  }

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ir::Document document;

  pdf2docx::Status extract_status = converter.ExtractIrFromFile(input_pdf, options, &document);
  if (!extract_status.ok()) {
    std::cerr << "Extract IR failed: " << extract_status.message() << "\n";
    return 2;
  }

  pdf2docx::Status html_status = pdf2docx::WriteIrToHtml(document, output_html, html_options);
  if (!html_status.ok()) {
    std::cerr << "Write HTML failed: " << html_status.message() << "\n";
    return 3;
  }

  std::cout << "IR to HTML done. pages=" << document.pages.size()
            << " scale=" << html_options.scale
            << " boxes=" << (html_options.show_boxes ? "on" : "off")
            << "\n";
  return 0;
}
