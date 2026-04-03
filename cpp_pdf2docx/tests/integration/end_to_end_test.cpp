#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "pdf2docx/converter.hpp"

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

bool FindEocd(const std::vector<uint8_t>& data, size_t* eocd_offset) {
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
  if (!FindEocd(zip_data, &eocd_offset)) {
    return false;
  }

  const uint16_t total_entries = ReadLe16(zip_data, eocd_offset + 10);
  size_t cursor = ReadLe32(zip_data, eocd_offset + 16);
  constexpr uint32_t kCdSignature = 0x02014b50;
  for (uint16_t i = 0; i < total_entries; ++i) {
    if (cursor + 46 > zip_data.size() || ReadLe32(zip_data, cursor) != kCdSignature) {
      return false;
    }
    const uint16_t name_len = ReadLe16(zip_data, cursor + 28);
    const uint16_t extra_len = ReadLe16(zip_data, cursor + 30);
    const uint16_t comment_len = ReadLe16(zip_data, cursor + 32);
    const size_t name_offset = cursor + 46;
    if (name_offset + name_len > zip_data.size()) {
      return false;
    }
    names->emplace_back(reinterpret_cast<const char*>(&zip_data[name_offset]), name_len);
    cursor = name_offset + name_len + extra_len + comment_len;
  }
  return true;
}

size_t CountPrefix(const std::vector<std::string>& names, const std::string& prefix) {
  size_t count = 0;
  for (const auto& name : names) {
    if (name.rfind(prefix, 0) == 0) {
      ++count;
    }
  }
  return count;
}

}  // namespace

int main() {
  const std::filesystem::path sample_pdf = PDF2DOCX_TEST_IMAGE_TEXT_PDF_PATH;
  if (!std::filesystem::exists(sample_pdf)) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "pdf2docx_integration_test";
  std::filesystem::create_directories(tmp_dir);

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ConvertStats stats;

  const std::filesystem::path docx_with_images = tmp_dir / "integration_with_images.docx";
  pdf2docx::Status status = converter.ConvertFile(sample_pdf.string(), docx_with_images.string(), options, &stats);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (!std::filesystem::exists(docx_with_images)) {
    return EXIT_FAILURE;
  }
  if (stats.page_count == 0 || stats.image_count == 0) {
    return EXIT_FAILURE;
  }

  std::vector<uint8_t> zip_data;
  if (!ReadWholeFile(docx_with_images, &zip_data)) {
    return EXIT_FAILURE;
  }
  std::vector<std::string> entry_names;
  if (!ListZipEntryNames(zip_data, &entry_names)) {
    return EXIT_FAILURE;
  }
  if (CountPrefix(entry_names, "word/media/") == 0) {
    return EXIT_FAILURE;
  }
  if (std::find(entry_names.begin(), entry_names.end(), "word/styles.xml") == entry_names.end()) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path docx_without_images = tmp_dir / "integration_no_images.docx";
  options.extract_images = false;
  pdf2docx::ConvertStats no_image_stats;
  status = converter.ConvertFile(sample_pdf.string(), docx_without_images.string(), options, &no_image_stats);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (!ReadWholeFile(docx_without_images, &zip_data)) {
    return EXIT_FAILURE;
  }
  entry_names.clear();
  if (!ListZipEntryNames(zip_data, &entry_names)) {
    return EXIT_FAILURE;
  }
  if (CountPrefix(entry_names, "word/media/") != 0) {
    return EXIT_FAILURE;
  }
  if (no_image_stats.image_count != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
