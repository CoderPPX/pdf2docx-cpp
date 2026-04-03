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

}  // namespace

int main() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_docx_anchor_test.docx";

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;
  page.images.push_back(pdf2docx::ir::ImageBlock{
      .id = "img1",
      .mime_type = "image/png",
      .extension = "png",
      .x = 42.0,
      .y = 180.0,
      .width = 120.0,
      .height = 90.0,
      .has_quad = true,
      .quad = pdf2docx::ir::Quad{
          .p0 = pdf2docx::ir::Point{100.0, 200.0},
          .p1 = pdf2docx::ir::Point{220.0, 220.0},
          .p2 = pdf2docx::ir::Point{90.0, 280.0},
          .p3 = pdf2docx::ir::Point{210.0, 300.0},
      },
      .data = {0x89, 'P', 'N', 'G'},
  });
  document.pages.push_back(std::move(page));

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";

  pdf2docx::docx::DocxWriteOptions options;
  options.use_anchored_images = true;
  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats, options).ok()) {
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
  if (document_xml.find("<wp:anchor") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<wp:positionH") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<wp:posOffset>1143000</wp:posOffset>") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("<wp:posOffset>6883400</wp:posOffset>") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (document_xml.find("rot=\"") == std::string::npos) {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
