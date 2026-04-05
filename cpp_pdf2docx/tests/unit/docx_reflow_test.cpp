#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "docx/p0_writer.hpp"
#include "pdf2docx/ir.hpp"
#include "pdf2docx/types.hpp"

#if PDF2DOCX_HAS_MINIZIP
#include <zlib.h>
#endif

namespace {

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP

struct ZipEntryLocation {
  uint16_t compression_method = 0;
  uint32_t compressed_size = 0;
  uint32_t uncompressed_size = 0;
  uint32_t local_header_offset = 0;
};

uint16_t ReadLe16(const std::vector<uint8_t>& data, size_t offset) {
  return static_cast<uint16_t>(data[offset]) |
         (static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t ReadLe32(const std::vector<uint8_t>& data, size_t offset) {
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
}

bool ReadWholeFile(const std::filesystem::path& path, std::vector<uint8_t>* out) {
  if (out == nullptr) {
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) {
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return !out->empty();
}

bool FindEndOfCentralDirectory(const std::vector<uint8_t>& data, size_t* eocd_offset) {
  if (eocd_offset == nullptr || data.size() < 22) {
    return false;
  }
  constexpr uint32_t kEocdSignature = 0x06054b50;
  const size_t search_limit = std::min<size_t>(data.size(), 22 + 0xFFFF);
  const size_t start = data.size() - search_limit;
  for (size_t pos = data.size() - 22 + 1; pos-- > start;) {
    if (ReadLe32(data, pos) == kEocdSignature) {
      *eocd_offset = pos;
      return true;
    }
  }
  return false;
}

bool FindZipEntry(const std::vector<uint8_t>& zip_data,
                  const std::string& entry_name,
                  ZipEntryLocation* out_location) {
  if (out_location == nullptr) {
    return false;
  }

  size_t eocd_offset = 0;
  if (!FindEndOfCentralDirectory(zip_data, &eocd_offset)) {
    return false;
  }

  const uint16_t total_entries = ReadLe16(zip_data, eocd_offset + 10);
  size_t cursor = ReadLe32(zip_data, eocd_offset + 16);
  constexpr uint32_t kCdSignature = 0x02014b50;
  for (uint16_t i = 0; i < total_entries; ++i) {
    if (cursor + 46 > zip_data.size() || ReadLe32(zip_data, cursor) != kCdSignature) {
      return false;
    }
    const uint16_t file_name_len = ReadLe16(zip_data, cursor + 28);
    const uint16_t extra_len = ReadLe16(zip_data, cursor + 30);
    const uint16_t comment_len = ReadLe16(zip_data, cursor + 32);
    const size_t file_name_offset = cursor + 46;
    if (file_name_offset + file_name_len > zip_data.size()) {
      return false;
    }

    const std::string current_name(
        reinterpret_cast<const char*>(&zip_data[file_name_offset]), file_name_len);
    if (current_name == entry_name) {
      out_location->compression_method = ReadLe16(zip_data, cursor + 10);
      out_location->compressed_size = ReadLe32(zip_data, cursor + 20);
      out_location->uncompressed_size = ReadLe32(zip_data, cursor + 24);
      out_location->local_header_offset = ReadLe32(zip_data, cursor + 42);
      return true;
    }
    cursor = file_name_offset + file_name_len + extra_len + comment_len;
  }
  return false;
}

bool ExtractZipEntry(const std::vector<uint8_t>& zip_data,
                     const ZipEntryLocation& location,
                     std::string* content) {
  if (content == nullptr) {
    return false;
  }

  constexpr uint32_t kLocalHeaderSignature = 0x04034b50;
  const size_t local_header_offset = location.local_header_offset;
  if (local_header_offset + 30 > zip_data.size()) {
    return false;
  }
  if (ReadLe32(zip_data, local_header_offset) != kLocalHeaderSignature) {
    return false;
  }

  const uint16_t file_name_len = ReadLe16(zip_data, local_header_offset + 26);
  const uint16_t extra_len = ReadLe16(zip_data, local_header_offset + 28);
  const size_t compressed_data_offset = local_header_offset + 30 + file_name_len + extra_len;
  if (compressed_data_offset + location.compressed_size > zip_data.size()) {
    return false;
  }

  const uint8_t* compressed_data = zip_data.data() + compressed_data_offset;
  if (location.compression_method == 0) {
    content->assign(reinterpret_cast<const char*>(compressed_data),
                    reinterpret_cast<const char*>(compressed_data + location.uncompressed_size));
    return true;
  }

  if (location.compression_method == 8) {
    z_stream stream = {};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed_data));
    stream.avail_in = location.compressed_size;
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
      return false;
    }

    std::vector<uint8_t> uncompressed(location.uncompressed_size);
    stream.next_out = reinterpret_cast<Bytef*>(uncompressed.data());
    stream.avail_out = location.uncompressed_size;
    const int inflate_status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (inflate_status != Z_STREAM_END) {
      return false;
    }

    content->assign(uncompressed.begin(), uncompressed.end());
    return true;
  }

  return false;
}

void ReplaceAll(std::string* text, const std::string& from, const std::string& to) {
  if (text == nullptr || from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text->find(from, pos)) != std::string::npos) {
    text->replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::vector<std::string> ExtractParagraphTexts(const std::string& document_xml) {
  std::vector<std::string> paragraphs;

  size_t paragraph_pos = 0;
  while ((paragraph_pos = document_xml.find("<w:p", paragraph_pos)) != std::string::npos) {
    const size_t paragraph_end = document_xml.find("</w:p>", paragraph_pos);
    if (paragraph_end == std::string::npos) {
      break;
    }
    const std::string paragraph_chunk =
        document_xml.substr(paragraph_pos, paragraph_end + std::string("</w:p>").size() - paragraph_pos);
    paragraph_pos = paragraph_end + std::string("</w:p>").size();

    std::string paragraph_text;
    size_t text_pos = 0;
    while ((text_pos = paragraph_chunk.find("<w:t", text_pos)) != std::string::npos) {
      const size_t text_start = paragraph_chunk.find('>', text_pos);
      if (text_start == std::string::npos) {
        break;
      }
      const size_t text_end = paragraph_chunk.find("</w:t>", text_start + 1);
      if (text_end == std::string::npos) {
        break;
      }
      paragraph_text.append(paragraph_chunk.substr(text_start + 1, text_end - text_start - 1));
      text_pos = text_end + std::string("</w:t>").size();
    }

    ReplaceAll(&paragraph_text, "&amp;", "&");
    ReplaceAll(&paragraph_text, "&lt;", "<");
    ReplaceAll(&paragraph_text, "&gt;", ">");

    if (!paragraph_text.empty()) {
      paragraphs.push_back(std::move(paragraph_text));
    }
  }
  return paragraphs;
}

#endif  // PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP

}  // namespace

int main() {
#if !(PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP)
  return EXIT_SUCCESS;
#else
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_docx_reflow_test.docx";

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "This is a wrapped", .x = 40.0, .y = 700.0, .length = 120.0, .has_bbox = true, .bbox = {40.0, 696.0, 120.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "paragraph line.", .x = 40.0, .y = 682.0, .length = 105.0, .has_bbox = true, .bbox = {40.0, 678.0, 105.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Another paragraph starts.", .x = 60.0, .y = 650.0, .length = 160.0, .has_bbox = true, .bbox = {60.0, 646.0, 160.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Continues here.", .x = 60.0, .y = 632.0, .length = 95.0, .has_bbox = true, .bbox = {60.0, 628.0, 95.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "1. Section heading", .x = 40.0, .y = 600.0, .length = 120.0, .has_bbox = true, .bbox = {40.0, 596.0, 120.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Body after heading.", .x = 40.0, .y = 582.0, .length = 120.0, .has_bbox = true, .bbox = {40.0, 578.0, 120.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "落笔指令：说明A。", .x = 40.0, .y = 560.0, .length = 140.0, .has_bbox = true, .bbox = {40.0, 556.0, 140.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "抬笔指令：说明B。", .x = 40.0, .y = 542.0, .length = 140.0, .has_bbox = true, .bbox = {40.0, 538.0, 140.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "坐标点（x，y）映射到0.1mm * 0.1mm网格。", .x = 40.0, .y = 520.0, .length = 260.0, .has_bbox = true, .bbox = {40.0, 516.0, 260.0, 12.0}});

  document.pages.push_back(std::move(page));

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";

  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats).ok()) {
    return EXIT_FAILURE;
  }

  std::vector<uint8_t> zip_data;
  if (!ReadWholeFile(output_path, &zip_data)) {
    return EXIT_FAILURE;
  }

  ZipEntryLocation document_entry;
  if (!FindZipEntry(zip_data, "word/document.xml", &document_entry)) {
    return EXIT_FAILURE;
  }

  std::string document_xml;
  if (!ExtractZipEntry(zip_data, document_entry, &document_xml)) {
    return EXIT_FAILURE;
  }

  if (document_xml.find("<m:oMathPara>") != std::string::npos) {
    return EXIT_FAILURE;
  }

  const std::vector<std::string> paragraphs = ExtractParagraphTexts(document_xml);
  if (paragraphs.size() < 7) {
    return EXIT_FAILURE;
  }

  if (paragraphs[0] != "This is a wrapped paragraph line.") {
    return EXIT_FAILURE;
  }
  if (paragraphs[1] != "Another paragraph starts. Continues here.") {
    return EXIT_FAILURE;
  }
  if (paragraphs[2] != "1. Section heading") {
    return EXIT_FAILURE;
  }
  if (paragraphs[3] != "Body after heading.") {
    return EXIT_FAILURE;
  }
  if (paragraphs[4] != "落笔指令：说明A。") {
    return EXIT_FAILURE;
  }
  if (paragraphs[5] != "抬笔指令：说明B。") {
    return EXIT_FAILURE;
  }
  if (paragraphs[6] != "坐标点（x，y）映射到0.1mm * 0.1mm网格。") {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
#endif
}
