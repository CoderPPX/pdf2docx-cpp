#include "pdf2docx/font.hpp"

#if PDF2DOCX_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#endif

namespace pdf2docx {

Status ProbeFreeType(FreeTypeProbeInfo* info) {
  FreeTypeProbeInfo local_info;
#if PDF2DOCX_HAS_FREETYPE
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) != 0) {
    return Status::Error(ErrorCode::kInternalError, "freetype init failed");
  }
  local_info.available = true;

  FT_Int major = 0;
  FT_Int minor = 0;
  FT_Int patch = 0;
  FT_Library_Version(library, &major, &minor, &patch);
  local_info.major_version = static_cast<int>(major);
  local_info.minor_version = static_cast<int>(minor);
  local_info.patch_version = static_cast<int>(patch);
  local_info.version = std::to_string(local_info.major_version) + "." +
                       std::to_string(local_info.minor_version) + "." +
                       std::to_string(local_info.patch_version);

  local_info.has_truetype_module = FT_Get_Module(library, "truetype") != nullptr;
  local_info.has_cff_module = FT_Get_Module(library, "cff") != nullptr;

  FT_Done_FreeType(library);
#else
  local_info.version = "unavailable";
#endif

  if (info != nullptr) {
    *info = local_info;
  }
  return Status::Ok();
}

}  // namespace pdf2docx
