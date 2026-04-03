#include <cstdlib>
#include <filesystem>
#include <string>

#include "pdftools/convert/pdf2docx.hpp"

int main() {
#ifndef PDFTOOLS_TEST_IMAGE_TEXT_PDF_PATH
  return EXIT_FAILURE;
#else
  namespace fs = std::filesystem;
  const fs::path fixture_pdf = PDFTOOLS_TEST_IMAGE_TEXT_PDF_PATH;
  if (!fs::exists(fixture_pdf)) {
    return EXIT_FAILURE;
  }

  const fs::path out_dir = fs::temp_directory_path() / "pdftools_m1_test";
  fs::create_directories(out_dir);
  const fs::path out_docx = out_dir / "m1_output.docx";
  const fs::path out_ir = out_dir / "m1_output_ir.json";
  std::error_code ec;
  fs::remove(out_docx, ec);
  fs::remove(out_ir, ec);

  pdftools::convert::PdfToDocxRequest request;
  request.input_pdf = fixture_pdf.string();
  request.output_docx = out_docx.string();
  request.dump_ir_json_path = out_ir.string();
  request.extract_images = true;
  request.use_anchored_images = false;
  request.overwrite = true;

  pdftools::convert::PdfToDocxResult result;
  pdftools::Status status = pdftools::convert::ConvertPdfToDocx(request, &result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (result.page_count == 0) {
    return EXIT_FAILURE;
  }
  if (!fs::exists(out_docx) || !fs::exists(out_ir)) {
    return EXIT_FAILURE;
  }
  if (fs::file_size(out_docx) == 0 || fs::file_size(out_ir) == 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
#endif
}

