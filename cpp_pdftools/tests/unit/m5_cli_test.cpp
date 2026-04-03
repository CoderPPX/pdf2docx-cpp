#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

#include "pdftools/cli.hpp"

int main() {
#ifndef PDFTOOLS_TEST_PDF_PATH
  return EXIT_FAILURE;
#else
  namespace fs = std::filesystem;
  const fs::path fixture_pdf = PDFTOOLS_TEST_PDF_PATH;
  if (!fs::exists(fixture_pdf)) {
    return EXIT_FAILURE;
  }

  std::ostringstream out;
  std::ostringstream err;

  {
    std::vector<std::string> args = {"pdftools", "--help"};
    if (pdftools::RunCli(args, out, err) != 0) {
      return EXIT_FAILURE;
    }
  }

  const fs::path out_dir = fs::temp_directory_path() / "pdftools_m5_test";
  fs::create_directories(out_dir);
  const fs::path text_out = out_dir / "cli_text.txt";
  const fs::path docx_out = out_dir / "cli_convert.docx";

  {
    std::vector<std::string> args = {
        "pdftools", "text", "extract", "--input", fixture_pdf.string(), "--output", text_out.string()};
    if (pdftools::RunCli(args, out, err) != 0) {
      return EXIT_FAILURE;
    }
  }
  if (!fs::exists(text_out) || fs::file_size(text_out) == 0) {
    return EXIT_FAILURE;
  }

  {
    std::vector<std::string> args = {"pdftools", "convert", "pdf2docx", "--input", fixture_pdf.string(),
                                     "--output", docx_out.string(), "--no-images"};
    if (pdftools::RunCli(args, out, err) != 0) {
      return EXIT_FAILURE;
    }
  }
  if (!fs::exists(docx_out) || fs::file_size(docx_out) == 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
#endif
}

