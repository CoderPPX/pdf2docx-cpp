#include <cstdlib>
#include <filesystem>

#include <podofo/podofo.h>

#include "pdftools/pdf/document_ops.hpp"

namespace {

uint32_t GetPageCount(const std::filesystem::path& pdf_path) {
  PoDoFo::PdfMemDocument doc;
  doc.Load(pdf_path.string());
  return doc.GetPages().GetCount();
}

}  // namespace

int main() {
#ifndef PDFTOOLS_TEST_PDF_PATH
  return EXIT_FAILURE;
#else
  namespace fs = std::filesystem;
  const fs::path fixture_pdf = PDFTOOLS_TEST_PDF_PATH;
#ifdef PDFTOOLS_TEST_ALT_PDF_PATH
  const fs::path alt_pdf = PDFTOOLS_TEST_ALT_PDF_PATH;
#else
  const fs::path alt_pdf = fixture_pdf;
#endif
  if (!fs::exists(fixture_pdf) || !fs::exists(alt_pdf)) {
    return EXIT_FAILURE;
  }

  const uint32_t fixture_pages = GetPageCount(fixture_pdf);
  const uint32_t alt_pages = GetPageCount(alt_pdf);
  if (fixture_pages == 0 || alt_pages == 0) {
    return EXIT_FAILURE;
  }

  const fs::path out_dir = fs::temp_directory_path() / "pdftools_m2_test";
  fs::create_directories(out_dir);
  const fs::path merged_pdf = out_dir / "merged.pdf";
  const fs::path deleted_pdf = out_dir / "deleted.pdf";
  const fs::path inserted_pdf = out_dir / "inserted.pdf";
  const fs::path replaced_pdf = out_dir / "replaced.pdf";

  pdftools::pdf::MergePdfRequest merge_request;
  merge_request.input_pdfs = {fixture_pdf.string(), alt_pdf.string()};
  merge_request.output_pdf = merged_pdf.string();
  merge_request.overwrite = true;
  pdftools::pdf::MergePdfResult merge_result;
  auto status = pdftools::pdf::MergePdf(merge_request, &merge_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (merge_result.output_page_count != fixture_pages + alt_pages) {
    return EXIT_FAILURE;
  }

  pdftools::pdf::DeletePageRequest delete_request;
  delete_request.input_pdf = merged_pdf.string();
  delete_request.output_pdf = deleted_pdf.string();
  delete_request.page_number = 1;
  delete_request.overwrite = true;
  pdftools::pdf::DeletePageResult delete_result;
  status = pdftools::pdf::DeletePage(delete_request, &delete_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (delete_result.output_page_count != merge_result.output_page_count - 1) {
    return EXIT_FAILURE;
  }

  pdftools::pdf::InsertPageRequest insert_request;
  insert_request.input_pdf = deleted_pdf.string();
  insert_request.output_pdf = inserted_pdf.string();
  insert_request.insert_at = 1;
  insert_request.source_pdf = fixture_pdf.string();
  insert_request.source_page_number = 1;
  insert_request.overwrite = true;
  pdftools::pdf::InsertPageResult insert_result;
  status = pdftools::pdf::InsertPage(insert_request, &insert_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (insert_result.output_page_count != merge_result.output_page_count) {
    return EXIT_FAILURE;
  }

  pdftools::pdf::ReplacePageRequest replace_request;
  replace_request.input_pdf = inserted_pdf.string();
  replace_request.output_pdf = replaced_pdf.string();
  replace_request.page_number = 1;
  replace_request.source_pdf = alt_pdf.string();
  replace_request.source_page_number = 1;
  replace_request.overwrite = true;
  pdftools::pdf::ReplacePageResult replace_result;
  status = pdftools::pdf::ReplacePage(replace_request, &replace_result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (replace_result.output_page_count != insert_result.output_page_count) {
    return EXIT_FAILURE;
  }

  if (GetPageCount(replaced_pdf) != replace_result.output_page_count) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
#endif
}

