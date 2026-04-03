#include <iostream>
#include <string>

#include "pdf2docx/converter.hpp"
#include "pdf2docx/ir_json.hpp"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: pdf2docx <input.pdf> <output.docx> [--dump-ir <out.json>] [--no-images] [--disable-font-fallback] [--docx-anchored]\n";
    return 1;
  }

  const std::string input_pdf = argv[1];
  const std::string output_docx = argv[2];

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  std::string dump_ir_path;
  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-images") {
      options.extract_images = false;
      continue;
    }
    if (arg == "--disable-font-fallback") {
      options.enable_font_fallback = false;
      continue;
    }
    if (arg == "--docx-anchored") {
      options.docx_use_anchored_images = true;
      continue;
    }
    if (arg == "--dump-ir") {
      if (i + 1 >= argc) {
        std::cerr << "--dump-ir requires an output path\n";
        return 1;
      }
      dump_ir_path = argv[++i];
      continue;
    }
    std::cerr << "Unknown argument: " << arg << "\n";
    return 1;
  }

  pdf2docx::ConvertStats stats;
  pdf2docx::ir::Document dump_ir_document;
  pdf2docx::ir::Document* dump_ir_ptr = dump_ir_path.empty() ? nullptr : &dump_ir_document;

  pdf2docx::Status status = converter.ConvertFile(input_pdf, output_docx, options, &stats, dump_ir_ptr);
  if (!status.ok()) {
    std::cerr << "Convert failed: " << status.message() << "\n";
    return 2;
  }

  if (!dump_ir_path.empty()) {
    pdf2docx::Status write_status = pdf2docx::WriteIrToJson(dump_ir_document, dump_ir_path);
    if (!write_status.ok()) {
      std::cerr << "Dump IR failed during write: " << write_status.message() << "\n";
      return 3;
    }
  }

  std::cout << "P1 convert done. backend=" << stats.backend
            << " xml=" << stats.xml_backend
            << " elapsed_ms=" << stats.elapsed_ms
            << " extract_ms=" << stats.extract_elapsed_ms
            << " pipeline_ms=" << stats.pipeline_elapsed_ms
            << " write_ms=" << stats.write_elapsed_ms
            << " pages=" << stats.page_count
            << " images=" << stats.image_count
            << " skipped_images=" << stats.skipped_image_count
            << " warnings=" << stats.warning_count << "\n";
  return 0;
}
