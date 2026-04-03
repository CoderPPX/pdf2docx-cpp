#include "pdftools/pdf/extract_ops.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
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

Status WriteOutputFile(const std::string& output_path, const std::string& content) {
  if (output_path.empty()) {
    return Status::Ok();
  }

  std::ofstream out(output_path, std::ios::binary);
  if (!out.is_open()) {
    return Status::Error(ErrorCode::kIoError, "failed to open output file", output_path);
  }
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  out.close();
  if (!out.good()) {
    return Status::Error(ErrorCode::kIoError, "failed to write output file", output_path);
  }
  return Status::Ok();
}

std::string QuoteShellArgPosix(const std::string& value) {
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

Status TryExtractTextWithPdfToTextFallback(const ExtractTextRequest& request, ExtractTextResult* result) {
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  std::string command = "pdftotext " + QuoteShellArgPosix(request.input_pdf) + " - 2>/dev/null";
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return Status::Error(ErrorCode::kUnsupportedFeature, "pdftotext fallback is not available");
  }

  std::string text;
  std::array<char, 4096> buffer{};
  while (true) {
    size_t read_size = std::fread(buffer.data(), 1, buffer.size(), pipe);
    if (read_size > 0) {
      text.append(buffer.data(), read_size);
    }
    if (read_size < buffer.size()) {
      if (std::feof(pipe) != 0) {
        break;
      }
      if (std::ferror(pipe) != 0) {
        (void)pclose(pipe);
        return Status::Error(ErrorCode::kIoError, "failed to read pdftotext output");
      }
    }
  }

  int exit_status = pclose(pipe);
  if (exit_status != 0) {
    return Status::Error(ErrorCode::kPdfParseFailed, "pdftotext fallback failed");
  }

  result->entries.clear();
  std::istringstream iss(text);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) {
      continue;
    }
    TextEntry entry;
    entry.page = 1;
    entry.x = 0.0;
    entry.y = 0.0;
    entry.length = static_cast<double>(line.size());
    entry.text = line;
    result->entries.push_back(std::move(entry));
  }

  result->entry_count = static_cast<uint32_t>(result->entries.size());
  result->page_count = text.empty() ? 0 : 1;
  result->used_fallback = true;
  result->extractor = "pdftotext";
  if (request.output_format == TextOutputFormat::kJson) {
    result->text = BuildJsonText(result->entries);
  } else {
    result->text = std::move(text);
  }

  return WriteOutputFile(request.output_path, result->text);
}

Status TryLoadDocument(PoDoFo::PdfMemDocument* document, const std::string& input_pdf, std::string* error_message) {
  if (document == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "document pointer is null");
  }

  struct LoadAttempt {
    PoDoFo::PdfLoadOptions options;
    const char* tag;
  };

  const LoadAttempt attempts[] = {
      {PoDoFo::PdfLoadOptions::None, "default"},
      {PoDoFo::PdfLoadOptions::LoadStreamsEagerly, "load_streams_eagerly"},
      {PoDoFo::PdfLoadOptions::SkipXRefRecovery, "skip_xref_recovery"},
      {PoDoFo::PdfLoadOptions::LoadStreamsEagerly | PoDoFo::PdfLoadOptions::SkipXRefRecovery,
       "eager_and_skip_xref_recovery"},
  };

  std::string last_error;
  for (const auto& attempt : attempts) {
    try {
      document->Load(input_pdf, attempt.options);
      return Status::Ok();
    } catch (const std::exception& e) {
      last_error = std::string(attempt.tag) + ": " + e.what();
    } catch (...) {
      last_error = std::string(attempt.tag) + ": unknown load error";
    }
  }

  if (error_message != nullptr) {
    *error_message = std::move(last_error);
  }
  return Status::Error(ErrorCode::kPdfParseFailed, "failed to load PDF with all PoDoFo strategies");
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

  std::string primary_error_message = "failed to extract text from PDF";

  try {
    PoDoFo::PdfMemDocument document;
    Status load_status = TryLoadDocument(&document, request.input_pdf, &primary_error_message);
    if (!load_status.ok()) {
      throw std::runtime_error(primary_error_message);
    }
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
    result->used_fallback = false;
    result->extractor = "podofo";
    if (request.output_format == TextOutputFormat::kJson) {
      result->text = BuildJsonText(entries);
    } else {
      result->text = BuildPlainText(entries);
    }

    return WriteOutputFile(request.output_path, result->text);
  } catch (const std::exception& e) {
    primary_error_message = e.what();
  } catch (...) {
    primary_error_message = "failed to extract text from PDF";
  }

  if (!request.best_effort) {
    return Status::Error(ErrorCode::kPdfParseFailed, primary_error_message);
  }

  // Fallback for malformed but still salvageable PDFs.
  Status fallback_status = TryExtractTextWithPdfToTextFallback(request, result);
  if (fallback_status.ok()) {
    return Status::Ok();
  }

  return Status::Error(ErrorCode::kPdfParseFailed, primary_error_message);
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
    std::string load_error_message;
    Status load_status = TryLoadDocument(&document, request.input_pdf, &load_error_message);
    if (!load_status.ok()) {
      if (request.best_effort) {
        result->attachments.clear();
        result->parse_failed = true;
        result->parser = "none";
        return Status::Ok();
      }
      return Status::Error(ErrorCode::kPdfParseFailed, load_error_message);
    }

    result->parse_failed = false;
    result->parser = "podofo";

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
