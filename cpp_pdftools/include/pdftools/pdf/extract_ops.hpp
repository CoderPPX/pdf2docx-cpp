#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pdftools/status.hpp"

namespace pdftools::pdf {

enum class TextOutputFormat {
  kPlainText = 0,
  kJson,
};

struct ExtractTextRequest {
  std::string input_pdf;
  std::string output_path;
  uint32_t page_start = 1;  // 1-based
  uint32_t page_end = 0;    // 0 means to the last page
  bool include_positions = false;
  TextOutputFormat output_format = TextOutputFormat::kPlainText;
  bool best_effort = true;  // true: allow fallback extractor, false: strict PoDoFo only
  bool overwrite = false;
};

struct TextEntry {
  uint32_t page = 0;
  double x = 0.0;
  double y = 0.0;
  double length = 0.0;
  std::string text;
};

struct ExtractTextResult {
  uint32_t page_count = 0;
  uint32_t entry_count = 0;
  std::string text;
  std::vector<TextEntry> entries;
  bool used_fallback = false;
  std::string extractor = "podofo";
};

Status ExtractText(const ExtractTextRequest& request, ExtractTextResult* result);

struct ExtractAttachmentsRequest {
  std::string input_pdf;
  std::string output_dir;
  bool best_effort = true;  // true: parse failure returns empty result; false: return error
  bool overwrite = false;
};

struct AttachmentInfo {
  std::string filename;
  std::string path;
  uint64_t size_bytes = 0;
};

struct ExtractAttachmentsResult {
  std::vector<AttachmentInfo> attachments;
  bool parse_failed = false;
  std::string parser = "podofo";
};

Status ExtractAttachments(const ExtractAttachmentsRequest& request, ExtractAttachmentsResult* result);

}  // namespace pdftools::pdf
