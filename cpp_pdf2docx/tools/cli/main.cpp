#include <iostream>
#include <string>

#include "pdf2docx/converter.hpp"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: pdf2docx <input.pdf> <output.docx>\n";
    return 1;
  }

  const std::string input_pdf = argv[1];
  const std::string output_docx = argv[2];

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ConvertStats stats;

  pdf2docx::Status status = converter.ConvertFile(input_pdf, output_docx, options, &stats);
  if (!status.ok()) {
    std::cerr << "Convert failed: " << status.message() << "\n";
    return 2;
  }

  std::cout << "P1 convert done. backend=" << stats.backend
            << " xml=" << stats.xml_backend
            << " elapsed_ms=" << stats.elapsed_ms << "\n";
  return 0;
}
