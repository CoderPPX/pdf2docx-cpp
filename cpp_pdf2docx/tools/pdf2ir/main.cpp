#include <iostream>
#include <string>

#include "pdf2docx/converter.hpp"
#include "pdf2docx/ir_json.hpp"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: pdf2ir <input.pdf> <output.json>\n";
    return 1;
  }

  const std::string input_pdf = argv[1];
  const std::string output_json = argv[2];

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ir::Document document;
  const pdf2docx::Status extract_status = converter.ExtractIrFromFile(input_pdf, options, &document);
  if (!extract_status.ok()) {
    std::cerr << "Extract IR failed: " << extract_status.message() << "\n";
    return 2;
  }

  const pdf2docx::Status write_status = pdf2docx::WriteIrToJson(document, output_json);
  if (!write_status.ok()) {
    std::cerr << "Write JSON failed: " << write_status.message() << "\n";
    return 3;
  }

  size_t span_count = 0;
  size_t image_count = 0;
  for (const auto& page : document.pages) {
    span_count += page.spans.size();
    image_count += page.images.size();
  }

  std::cout << "IR written: pages=" << document.pages.size()
            << " spans=" << span_count
            << " images=" << image_count
            << " -> " << output_json << "\n";
  return 0;
}
