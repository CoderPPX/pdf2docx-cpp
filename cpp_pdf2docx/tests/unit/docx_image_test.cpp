#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "docx/p0_writer.hpp"
#include "pdf2docx/ir.hpp"
#include "pdf2docx/types.hpp"

namespace {

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

bool ListZipEntryNames(const std::vector<uint8_t>& zip_data, std::vector<std::string>* names) {
  if (names == nullptr) {
    return false;
  }
  names->clear();

  size_t eocd_offset = 0;
  if (!FindEndOfCentralDirectory(zip_data, &eocd_offset)) {
    return false;
  }

  const uint16_t total_entries = ReadLe16(zip_data, eocd_offset + 10);
  const uint32_t central_directory_offset = ReadLe32(zip_data, eocd_offset + 16);
  size_t cursor = central_directory_offset;
  constexpr uint32_t kCdSignature = 0x02014b50;

  for (uint16_t entry_index = 0; entry_index < total_entries; ++entry_index) {
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
    names->emplace_back(reinterpret_cast<const char*>(&zip_data[file_name_offset]), file_name_len);
    cursor = file_name_offset + file_name_len + extra_len + comment_len;
  }

  return true;
}

}  // namespace

int main() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "pdf2docx_docx_image_test.docx";

  pdf2docx::ConvertStats stats;
  stats.backend = "podofo";
  stats.xml_backend = "tinyxml2";

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.width_pt = 595.0;
  page.height_pt = 842.0;
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "image test", .x = 10.0, .y = 20.0, .length = 20.0});
  page.images.push_back(pdf2docx::ir::ImageBlock{
      .id = "img1",
      .mime_type = "image/jpeg",
      .extension = "jpg",
      .x = 30.0,
      .y = 100.0,
      .width = 120.0,
      .height = 80.0,
      .data = {0xFF, 0xD8, 0xFF, 0xD9},
  });
  document.pages.push_back(std::move(page));

  pdf2docx::docx::P0Writer writer;
  if (!writer.WriteFromIr(document, output_path.string(), stats).ok()) {
    return EXIT_FAILURE;
  }

  std::vector<uint8_t> zip_data;
  if (!ReadWholeFile(output_path, &zip_data)) {
    return EXIT_FAILURE;
  }

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  std::vector<std::string> entry_names;
  if (!ListZipEntryNames(zip_data, &entry_names)) {
    return EXIT_FAILURE;
  }

  size_t media_file_count = 0;
  bool has_document_rels = false;
  bool has_styles_xml = false;
  for (const auto& entry_name : entry_names) {
    if (entry_name.rfind("word/media/", 0) == 0) {
      ++media_file_count;
    }
    if (entry_name == "word/_rels/document.xml.rels") {
      has_document_rels = true;
    }
    if (entry_name == "word/styles.xml") {
      has_styles_xml = true;
    }
  }

  if (media_file_count != 1) {
    return EXIT_FAILURE;
  }
  if (!has_document_rels) {
    return EXIT_FAILURE;
  }
  if (!has_styles_xml) {
    return EXIT_FAILURE;
  }
#else
  const std::string blob(zip_data.begin(), zip_data.end());
  if (blob.find("placeholder") == std::string::npos) {
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}
