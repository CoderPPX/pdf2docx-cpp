#pragma once

namespace pdf2docx {

struct BuildInfo {
  static constexpr bool has_zlib =
#if PDF2DOCX_HAS_ZLIB
      true;
#else
      false;
#endif

  static constexpr bool has_tinyxml2 =
#if PDF2DOCX_HAS_TINYXML2
      true;
#else
      false;
#endif

  static constexpr bool has_minizip =
#if PDF2DOCX_HAS_MINIZIP
      true;
#else
      false;
#endif

  static constexpr bool has_freetype =
#if PDF2DOCX_HAS_FREETYPE
      true;
#else
      false;
#endif

  static constexpr bool has_podofo =
#if PDF2DOCX_HAS_PODOFO
      true;
#else
      false;
#endif
};

}  // namespace pdf2docx
