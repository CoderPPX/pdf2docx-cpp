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
  pdf2docx::ir::Document extracted_document;
  pdf2docx::Status status =
      converter.ConvertFile(input_pdf.string(), output_docx.string(), options, &stats, &extracted_document);
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
  if (stats.page_count != extracted_document.pages.size()) {
    return EXIT_FAILURE;
  }
  if (stats.image_count == 0) {
    return EXIT_FAILURE;
  }
  size_t extracted_image_count = 0;
  for (const auto& page : extracted_document.pages) {
    extracted_image_count += page.images.size();
  }
  if (stats.image_count != extracted_image_count) {
    return EXIT_FAILURE;
  }
  if (stats.extracted_image_count != stats.image_count) {
    return EXIT_FAILURE;
  }
  if (stats.backend_warning_count != stats.warning_count) {
    return EXIT_FAILURE;
  }
  if (stats.extract_elapsed_ms < 0.0 || stats.pipeline_elapsed_ms < 0.0 || stats.write_elapsed_ms < 0.0) {
    return EXIT_FAILURE;
  }
  if (stats.elapsed_ms < stats.extract_elapsed_ms ||
      stats.elapsed_ms < stats.pipeline_elapsed_ms ||
      stats.elapsed_ms < stats.write_elapsed_ms) {
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

  const std::filesystem::path input_image_text_pdf = PDF2DOCX_TEST_IMAGE_TEXT_PDF_PATH;
  if (std::filesystem::exists(input_image_text_pdf)) {
    const std::filesystem::path image_text_docx = tmp_dir / "image_text_output.docx";
    pdf2docx::ConvertStats image_text_stats;
    status = converter.ConvertFile(input_image_text_pdf.string(), image_text_docx.string(), options, &image_text_stats);
    if (!status.ok()) {
      return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(image_text_docx)) {
      return EXIT_FAILURE;
    }
    if (image_text_stats.extracted_image_count != image_text_stats.image_count) {
      return EXIT_FAILURE;
    }
    if (image_text_stats.warning_count < image_text_stats.skipped_image_count) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
