#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace pdf2docx {

struct FontResolveRequest {
  std::string family;
  bool bold = false;
  bool italic = false;
  uint32_t codepoint = 0;
};

struct FontResolveResult {
  std::string requested_family;
  std::string resolved_family;
  std::string file_path;
  bool fallback_used = false;
  bool found = false;
};

class FontResolver {
 public:
  void SetAlias(const std::string& alias, const std::string& canonical_family);
  void SetFallbackFamilies(std::vector<std::string> fallback_families);
  void AddFontPath(const std::string& family, const std::string& file_path);
  FontResolveResult Resolve(const FontResolveRequest& request) const;

 private:
  std::vector<std::pair<std::string, std::string>> aliases_;
  std::vector<std::string> fallback_families_;
  std::vector<std::pair<std::string, std::string>> family_file_paths_;
};

}  // namespace pdf2docx
