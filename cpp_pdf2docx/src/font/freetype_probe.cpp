#include "pdf2docx/font.hpp"

#if PDF2DOCX_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace pdf2docx {

Status ProbeFreeType() {
#if PDF2DOCX_HAS_FREETYPE
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) != 0) {
    return Status::Error(ErrorCode::kInternalError, "freetype init failed");
  }
  FT_Done_FreeType(library);
#endif
  return Status::Ok();
}

}  // namespace pdf2docx
