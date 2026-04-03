#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "pdf2docx/converter.hpp"

int main() {
  const std::filesystem::path input_pdf = PDF2DOCX_TEST_PDF_PATH;
  if (!std::filesystem::exists(input_pdf)) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "pdf2docx_converter_test";
  std::filesystem::create_directories(tmp_dir);
  const std::filesystem::path output_docx = tmp_dir / "output.docx";

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ConvertStats stats;
  pdf2docx::Status status = converter.ConvertFile(input_pdf.string(), output_docx.string(), options, &stats);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  if (!std::filesystem::exists(output_docx)) {
    return EXIT_FAILURE;
  }

  if (stats.backend != "podofo") {
    return EXIT_FAILURE;
  }

  if (stats.xml_backend.empty()) {
    return EXIT_FAILURE;
  }

  if (stats.page_count == 0) {
    return EXIT_FAILURE;
  }
  if (stats.image_count == 0) {
    return EXIT_FAILURE;
  }

  std::ifstream stream(output_docx, std::ios::binary);
  if (!stream.is_open()) {
    return EXIT_FAILURE;
  }

  char first = '\0';
  char second = '\0';
  stream.read(&first, 1);
  stream.read(&second, 1);

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  if (!(first == 'P' && second == 'K')) {
    return EXIT_FAILURE;
  }
#else
  if (first != 'p') {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
