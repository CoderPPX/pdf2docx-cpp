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

#if PDF2DOCX_HAS_MINIZIP
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
#endif

  return false;
}

size_t CountSubstring(const std::string& text, const std::string& needle) {
  if (needle.empty()) {
    return 0;
  }
  size_t count = 0;
  size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

}  // namespace

int main() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_docx_math_test.docx";

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "x", .x = 80.0, .y = 700.0, .length = 8.0, .has_bbox = true, .bbox = {80.0, 698.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "2", .x = 90.0, .y = 706.0, .length = 6.0, .has_bbox = true, .bbox = {90.0, 706.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "+", .x = 103.0, .y = 700.0, .length = 7.0, .has_bbox = true, .bbox = {103.0, 698.0, 7.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "y", .x = 114.0, .y = 700.0, .length = 8.0, .has_bbox = true, .bbox = {114.0, 698.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "1", .x = 124.0, .y = 694.0, .length = 6.0, .has_bbox = true, .bbox = {124.0, 694.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "=", .x = 136.0, .y = 700.0, .length = 8.0, .has_bbox = true, .bbox = {136.0, 698.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "z", .x = 149.0, .y = 700.0, .length = 8.0, .has_bbox = true, .bbox = {149.0, 698.0, 8.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "a", .x = 80.0, .y = 660.0, .length = 8.0, .has_bbox = true, .bbox = {80.0, 658.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "/", .x = 91.0, .y = 660.0, .length = 6.0, .has_bbox = true, .bbox = {91.0, 658.0, 6.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "b", .x = 101.0, .y = 660.0, .length = 8.0, .has_bbox = true, .bbox = {101.0, 658.0, 8.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "A ∩ B = {x | x ∈ A}", .x = 80.0, .y = 640.0, .length = 88.0, .has_bbox = true, .bbox = {80.0, 638.0, 88.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "f(x) = x + 1", .x = 80.0, .y = 602.0, .length = 56.0, .has_bbox = true, .bbox = {80.0, 600.0, 56.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "= 2x + 1", .x = 88.0, .y = 590.0, .length = 48.0, .has_bbox = true, .bbox = {88.0, 588.0, 48.0, 12.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "a + b = c", .x = 80.0, .y = 572.0, .length = 48.0, .has_bbox = true, .bbox = {80.0, 570.0, 48.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "2", .x = 85.0, .y = 578.0, .length = 6.0, .has_bbox = true, .bbox = {85.0, 578.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "2", .x = 101.0, .y = 578.0, .length = 6.0, .has_bbox = true, .bbox = {101.0, 578.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "2", .x = 123.0, .y = 578.0, .length = 6.0, .has_bbox = true, .bbox = {123.0, 578.0, 6.0, 8.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "∫", .x = 80.0, .y = 528.0, .length = 8.0, .has_bbox = true, .bbox = {80.0, 526.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "0", .x = 88.0, .y = 522.0, .length = 6.0, .has_bbox = true, .bbox = {88.0, 522.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "1", .x = 88.0, .y = 534.0, .length = 6.0, .has_bbox = true, .bbox = {88.0, 534.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "x", .x = 98.0, .y = 528.0, .length = 8.0, .has_bbox = true, .bbox = {98.0, 526.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "2", .x = 107.0, .y = 534.0, .length = 6.0, .has_bbox = true, .bbox = {107.0, 534.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "dx", .x = 115.0, .y = 528.0, .length = 14.0, .has_bbox = true, .bbox = {115.0, 526.0, 14.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "=", .x = 132.0, .y = 528.0, .length = 8.0, .has_bbox = true, .bbox = {132.0, 526.0, 8.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "1", .x = 143.0, .y = 534.0, .length = 6.0, .has_bbox = true, .bbox = {143.0, 534.0, 6.0, 8.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "3", .x = 143.0, .y = 522.0, .length = 6.0, .has_bbox = true, .bbox = {143.0, 522.0, 6.0, 8.0}});

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Normal", .x = 80.0, .y = 620.0, .length = 30.0, .has_bbox = true, .bbox = {80.0, 618.0, 30.0, 12.0}});
  page.spans.push_back(pdf2docx::ir::TextSpan{
      .text = "paragraph",
      .x = 114.0,
      .y = 620.0,
      .length = 45.0,
      .has_bbox = true,
      .bbox = {114.0, 618.0, 45.0, 12.0},
  });

  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "using", .x = 80.0, .y = 600.0, .length = 26.0, .has_bbox = true, .bbox = {80.0, 598.0, 26.0, 12.0}});
  page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "samples", .x = 110.0, .y = 600.0, .length = 40.0, .has_bbox = true, .bbox = {110.0, 598.0, 40.0, 12.0}});

  document.pages.push_back(std::move(page));

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";

  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats).ok()) {
    return EXIT_FAILURE;
  }

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
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

  if (document_xml.find("xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\"") ==
      std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<m:oMathPara") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (CountSubstring(document_xml, "<m:oMathPara") != 6) {
    return EXIT_FAILURE;
  }
  if (CountSubstring(document_xml, "<m:sSup") < 3) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<m:sSubSup") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<m:sSub") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<m:f") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("∩") == std::string::npos || document_xml.find("∈") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("Normal paragraph") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("using samples") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<m:t>using</m:t>") != std::string::npos) {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
