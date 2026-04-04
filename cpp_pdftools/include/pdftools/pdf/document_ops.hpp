#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pdftools/status.hpp"

namespace pdftools::pdf {

struct MergePdfRequest {
  std::vector<std::string> input_pdfs;
  std::string output_pdf;
  bool overwrite = false;
};

struct MergePdfResult {
  uint32_t output_page_count = 0;
};

Status MergePdf(const MergePdfRequest& request, MergePdfResult* result);

struct PdfInfoRequest {
  std::string input_pdf;
};

struct PdfInfoResult {
  uint32_t page_count = 0;
};

Status GetPdfInfo(const PdfInfoRequest& request, PdfInfoResult* result);

struct DeletePageRequest {
  std::string input_pdf;
  std::string output_pdf;
  uint32_t page_number = 0;  // 1-based
  bool overwrite = false;
};

struct DeletePageResult {
  uint32_t output_page_count = 0;
};

Status DeletePage(const DeletePageRequest& request, DeletePageResult* result);

struct InsertPageRequest {
  std::string input_pdf;
  std::string output_pdf;
  uint32_t insert_at = 0;  // 1-based position in output
  std::string source_pdf;
  uint32_t source_page_number = 0;  // 1-based
  bool overwrite = false;
};

struct InsertPageResult {
  uint32_t output_page_count = 0;
};

Status InsertPage(const InsertPageRequest& request, InsertPageResult* result);

struct ReplacePageRequest {
  std::string input_pdf;
  std::string output_pdf;
  uint32_t page_number = 0;  // 1-based
  std::string source_pdf;
  uint32_t source_page_number = 0;  // 1-based
  bool overwrite = false;
};

struct ReplacePageResult {
  uint32_t output_page_count = 0;
};

Status ReplacePage(const ReplacePageRequest& request, ReplacePageResult* result);

}  // namespace pdftools::pdf
