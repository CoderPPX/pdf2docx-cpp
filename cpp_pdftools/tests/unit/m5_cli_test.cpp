#include <cstdlib>
#include <filesystem>
#include <fstream>
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
  const fs::path invalid_input = out_dir / "not_pdf.bin";
  const fs::path attachments_out_dir = out_dir / "attachments";
  std::error_code ec;
  fs::remove(text_out, ec);
  fs::remove(docx_out, ec);
  fs::remove_all(attachments_out_dir, ec);
  {
    std::ofstream invalid_out(invalid_input, std::ios::binary);
    invalid_out << "not-a-pdf";
  }

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
  if (out.str().find("extractor=") == std::string::npos) {
    return EXIT_FAILURE;
  }

  const fs::path strict_text_out = out_dir / "cli_text_strict.txt";
  fs::remove(strict_text_out, ec);
  {
    std::vector<std::string> args = {"pdftools", "text",   "extract", "--input", fixture_pdf.string(),
                                     "--output", strict_text_out.string(), "--strict"};
    if (pdftools::RunCli(args, out, err) != 0) {
      return EXIT_FAILURE;
    }
  }
  if (!fs::exists(strict_text_out) || fs::file_size(strict_text_out) == 0) {
    return EXIT_FAILURE;
  }

  {
    std::vector<std::string> args = {"pdftools", "attachments", "extract", "--input", fixture_pdf.string(),
                                     "--out-dir", attachments_out_dir.string()};
    if (pdftools::RunCli(args, out, err) != 0) {
      return EXIT_FAILURE;
    }
  }
  if (out.str().find("parse_failed=no") == std::string::npos) {
    return EXIT_FAILURE;
  }

  {
    std::vector<std::string> args = {"pdftools", "attachments", "extract", "--input", invalid_input.string(),
                                     "--out-dir", (out_dir / "attachments_invalid").string(), "--strict"};
    if (pdftools::RunCli(args, out, err) == 0) {
      return EXIT_FAILURE;
    }
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
