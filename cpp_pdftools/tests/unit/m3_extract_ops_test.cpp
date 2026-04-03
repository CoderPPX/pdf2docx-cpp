#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <podofo/podofo.h>

#include "pdftools/pdf/extract_ops.hpp"

int main() {
#ifndef PDFTOOLS_TEST_PDF_PATH
  return EXIT_FAILURE;
#else
  namespace fs = std::filesystem;
  const fs::path fixture_pdf = PDFTOOLS_TEST_PDF_PATH;
  if (!fs::exists(fixture_pdf)) {
    return EXIT_FAILURE;
  }

  const fs::path out_dir = fs::temp_directory_path() / "pdftools_m3_test";
  fs::create_directories(out_dir);

  // Text extraction scenario
  const fs::path text_output = out_dir / "extract.txt";
  pdftools::pdf::ExtractTextRequest text_request;
  text_request.input_pdf = fixture_pdf.string();
  text_request.output_path = text_output.string();
  text_request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  text_request.overwrite = true;

  pdftools::pdf::ExtractTextResult text_result;
  pdftools::Status status = pdftools::pdf::ExtractText(text_request, &text_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (text_result.page_count == 0 || text_result.entry_count == 0) {
    return EXIT_FAILURE;
  }
  if (!fs::exists(text_output) || fs::file_size(text_output) == 0) {
    return EXIT_FAILURE;
  }

  // Attachment extraction scenario with generated fixture PDF.
  const fs::path attachment_pdf = out_dir / "attachment_fixture.pdf";
  const fs::path attachment_dir = out_dir / "attachments";
  fs::create_directories(attachment_dir);

  {
    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage();
    std::shared_ptr<PoDoFo::PdfFileSpec> file_spec = doc.CreateFileSpec();
    file_spec->SetFilename(PoDoFo::PdfString("sample.txt"));
    const std::string payload = "hello-from-attachment";
    PoDoFo::charbuff content(payload);
    file_spec->SetEmbeddedData(content);
    auto& names = doc.GetOrCreateNames();
    auto& embedded_files = names.GetOrCreateTree<PoDoFo::PdfEmbeddedFiles>();
    embedded_files.AddValue(*file_spec->GetFilename(), file_spec);
    doc.Save(attachment_pdf.string());
  }

  pdftools::pdf::ExtractAttachmentsRequest attachment_request;
  attachment_request.input_pdf = attachment_pdf.string();
  attachment_request.output_dir = attachment_dir.string();
  attachment_request.overwrite = true;

  pdftools::pdf::ExtractAttachmentsResult attachment_result;
  status = pdftools::pdf::ExtractAttachments(attachment_request, &attachment_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (attachment_result.attachments.empty()) {
    return EXIT_FAILURE;
  }

  const fs::path extracted_file = attachment_result.attachments.front().path;
  if (!fs::exists(extracted_file) || fs::file_size(extracted_file) == 0) {
    return EXIT_FAILURE;
  }

  std::ifstream in(extracted_file, std::ios::binary);
  std::string loaded((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (loaded != "hello-from-attachment") {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
#endif
}
