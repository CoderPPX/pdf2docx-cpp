#include "pdf2docx/font_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace pdf2docx {

namespace {

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

}  // namespace

void FontResolver::SetAlias(const std::string& alias, const std::string& canonical_family) {
  if (alias.empty() || canonical_family.empty()) {
    return;
  }

  const std::string alias_key = ToLowerAscii(alias);
  for (auto& entry : aliases_) {
    if (entry.first == alias_key) {
      entry.second = canonical_family;
      return;
    }
  }
  aliases_.push_back({alias_key, canonical_family});
}

void FontResolver::SetFallbackFamilies(std::vector<std::string> fallback_families) {
  fallback_families_.clear();
  fallback_families_.reserve(fallback_families.size());
  for (auto& family : fallback_families) {
    if (!family.empty()) {
      fallback_families_.push_back(std::move(family));
    }
  }
}

void FontResolver::AddFontPath(const std::string& family, const std::string& file_path) {
  if (family.empty() || file_path.empty()) {
    return;
  }
  const std::string key = ToLowerAscii(family);
  for (auto& entry : family_file_paths_) {
    if (entry.first == key) {
      entry.second = file_path;
      return;
    }
  }
  family_file_paths_.push_back({key, file_path});
}

FontResolveResult FontResolver::Resolve(const FontResolveRequest& request) const {
  FontResolveResult result;
  result.requested_family = request.family;

  std::string resolved_family = request.family;
  if (!resolved_family.empty()) {
    const std::string request_key = ToLowerAscii(resolved_family);
    for (const auto& alias : aliases_) {
      if (alias.first == request_key) {
        resolved_family = alias.second;
        break;
      }
    }
  }

  if (resolved_family.empty() && !fallback_families_.empty()) {
    resolved_family = fallback_families_.front();
    result.fallback_used = true;
  }

  result.resolved_family = resolved_family;
  result.found = !resolved_family.empty();
  if (!result.found) {
    return result;
  }

  const std::string resolved_key = ToLowerAscii(resolved_family);
  for (const auto& family_file_path : family_file_paths_) {
    if (family_file_path.first == resolved_key) {
      result.file_path = family_file_path.second;
      break;
    }
  }
  return result;
}

}  // namespace pdf2docx
