#include "pdftools/pdf/extract_ops.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <podofo/podofo.h>

namespace pdftools::pdf {

namespace {

Status PrepareOutputPath(const std::string& output_path, bool overwrite) {
  if (output_path.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output path must not be empty");
  }

  std::filesystem::path out(output_path);
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && !overwrite) {
    return Status::Error(ErrorCode::kAlreadyExists, "output path already exists", output_path);
  }

  auto parent = out.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, "failed to create output directory", parent.string());
    }
  }

  return Status::Ok();
}

std::string EscapeJsonString(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 16);
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

Status ResolvePageRange(uint32_t total_pages,
                        uint32_t page_start,
                        uint32_t page_end,
                        uint32_t* first_page_zero_based,
                        uint32_t* last_page_zero_based) {
  if (first_page_zero_based == nullptr || last_page_zero_based == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "null page range output pointer");
  }
  if (total_pages == 0) {
    return Status::Error(ErrorCode::kPdfParseFailed, "input document has no pages");
  }

  if (page_start == 0 || page_start > total_pages) {
    return Status::Error(ErrorCode::kInvalidArgument, "page_start out of range");
  }

  uint32_t normalized_end = page_end == 0 ? total_pages : page_end;
  if (normalized_end < page_start || normalized_end > total_pages) {
    return Status::Error(ErrorCode::kInvalidArgument, "page_end out of range");
  }

  *first_page_zero_based = page_start - 1;
  *last_page_zero_based = normalized_end - 1;
  return Status::Ok();
}

std::string BuildPlainText(const std::vector<TextEntry>& entries) {
  std::ostringstream oss;
  uint32_t current_page = 0;
  for (const auto& entry : entries) {
    if (entry.page != current_page) {
      current_page = entry.page;
      if (oss.tellp() > 0) {
        oss << "\n";
      }
      oss << "=== Page " << current_page << " ===\n";
    }
    oss << entry.text << "\n";
  }
  return oss.str();
}

std::string BuildJsonText(const std::vector<TextEntry>& entries) {
  std::ostringstream oss;
  oss << "{\n  \"entries\": [\n";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    oss << "    {\"page\":" << entry.page
        << ",\"x\":" << entry.x
        << ",\"y\":" << entry.y
        << ",\"length\":" << entry.length
        << ",\"text\":\"" << EscapeJsonString(entry.text) << "\"}";
    if (i + 1 < entries.size()) {
      oss << ",";
    }
    oss << "\n";
  }
  oss << "  ]\n}\n";
  return oss.str();
}

std::string SanitizeFilename(std::string name) {
  if (name.empty()) {
    return "attachment.bin";
  }

  for (char& ch : name) {
    if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
        ch == '|') {
      ch = '_';
    }
  }
  return name;
}

std::filesystem::path ResolveUniqueOutputPath(const std::filesystem::path& base_path, bool overwrite) {
  if (overwrite || !std::filesystem::exists(base_path)) {
    return base_path;
  }

  std::filesystem::path stem = base_path.stem();
  std::filesystem::path ext = base_path.extension();
  std::filesystem::path parent = base_path.parent_path();
  for (int i = 1; i < 100000; ++i) {
    std::filesystem::path candidate = parent / (stem.string() + "_" + std::to_string(i) + ext.string());
    if (!std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return base_path;
}

}  // namespace

Status ExtractText(const ExtractTextRequest& request, ExtractTextResult* result) {
  if (request.input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }
  if (!request.output_path.empty()) {
    Status output_status = PrepareOutputPath(request.output_path, request.overwrite);
    if (!output_status.ok()) {
      return output_status;
    }
  }

  try {
    PoDoFo::PdfMemDocument document;
    document.Load(request.input_pdf);
    const uint32_t total_pages = document.GetPages().GetCount();

    uint32_t begin_page = 0;
    uint32_t end_page = 0;
    Status range_status =
        ResolvePageRange(total_pages, request.page_start, request.page_end, &begin_page, &end_page);
    if (!range_status.ok()) {
      return range_status;
    }

    std::vector<TextEntry> entries;
    for (uint32_t page_index = begin_page; page_index <= end_page; ++page_index) {
      const auto& page = document.GetPages().GetPageAt(page_index);
      std::vector<PoDoFo::PdfTextEntry> page_entries;
      PoDoFo::PdfTextExtractParams params;
      page.ExtractTextTo(page_entries, params);

      for (const auto& page_entry : page_entries) {
        TextEntry entry;
        entry.page = page_index + 1;
        entry.x = page_entry.X;
        entry.y = page_entry.Y;
        entry.length = page_entry.Length;
        entry.text = page_entry.Text;
        entries.push_back(std::move(entry));
      }
    }

    result->entries = entries;
    result->entry_count = static_cast<uint32_t>(entries.size());
    result->page_count = end_page - begin_page + 1;
    if (request.output_format == TextOutputFormat::kJson) {
      result->text = BuildJsonText(entries);
    } else {
      result->text = BuildPlainText(entries);
    }

    if (!request.output_path.empty()) {
      std::ofstream out(request.output_path, std::ios::binary);
      if (!out.is_open()) {
        return Status::Error(ErrorCode::kIoError, "failed to open output file", request.output_path);
      }
      out.write(result->text.data(), static_cast<std::streamsize>(result->text.size()));
      out.close();
      if (!out.good()) {
        return Status::Error(ErrorCode::kIoError, "failed to write output file", request.output_path);
      }
    }

    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to extract text from PDF");
  }
}

Status ExtractAttachments(const ExtractAttachmentsRequest& request, ExtractAttachmentsResult* result) {
  if (request.input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (request.output_dir.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output_dir must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  std::filesystem::path output_dir(request.output_dir);
  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  if (ec) {
    return Status::Error(ErrorCode::kIoError, "failed to create output directory", request.output_dir);
  }

  try {
    PoDoFo::PdfMemDocument document;
    document.Load(request.input_pdf);

    PoDoFo::PdfNameTrees* names = document.GetNames();
    if (names == nullptr) {
      return Status::Ok();
    }

    PoDoFo::PdfEmbeddedFiles* tree = names->GetTree<PoDoFo::PdfEmbeddedFiles>();
    if (tree == nullptr) {
      return Status::Ok();
    }

    PoDoFo::PdfNameTree<PoDoFo::PdfFileSpec>::Map embedded_files;
    tree->ToDictionary(embedded_files);

    for (const auto& [key, file_spec] : embedded_files) {
      if (!file_spec) {
        continue;
      }
      auto data = file_spec->GetEmbeddedData();
      if (!data.has_value()) {
        continue;
      }

      std::string filename = std::string(key.GetString());
      if (filename.empty()) {
        auto maybe_name = file_spec->GetFilename();
        if (maybe_name.has_value()) {
          filename = std::string(maybe_name->GetString());
        }
      }
      filename = SanitizeFilename(filename);
      std::filesystem::path out_path = ResolveUniqueOutputPath(output_dir / filename, request.overwrite);

      std::ofstream out(out_path, std::ios::binary);
      if (!out.is_open()) {
        return Status::Error(ErrorCode::kIoError, "failed to open attachment output file", out_path.string());
      }
      out.write(data->data(), static_cast<std::streamsize>(data->size()));
      out.close();
      if (!out.good()) {
        return Status::Error(ErrorCode::kIoError, "failed to write attachment file", out_path.string());
      }

      AttachmentInfo info;
      info.filename = filename;
      info.path = out_path.string();
      info.size_bytes = static_cast<uint64_t>(data->size());
      result->attachments.push_back(std::move(info));
    }

    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to extract attachments from PDF");
  }
}

}  // namespace pdftools::pdf
