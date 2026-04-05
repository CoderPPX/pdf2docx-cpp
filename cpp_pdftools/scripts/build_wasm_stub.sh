#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CPP_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_ROOT="$(cd "${CPP_ROOT}/.." && pwd)"

OUT_DIR="${OUT_DIR:-${PROJECT_ROOT}/web_pdftools/wasm/dist}"
OUT_JS="${OUT_DIR}/pdftools_wasm.mjs"
BUILD_TYPE="${BUILD_TYPE:-Release}"
USE_CCACHE="${USE_CCACHE:-auto}"
THIRDPARTY_WASM_ROOT="${THIRDPARTY_WASM_ROOT:-${PROJECT_ROOT}/thirdparty/build_wasm}"
CCACHE_DIR_PATH="${CCACHE_DIR:-${THIRDPARTY_WASM_ROOT}/.ccache}"

PODOFO_LIB="${THIRDPARTY_WASM_ROOT}/podofo_noopenssl_xml2_iconv/src/podofo/libpodofo.a"
PODOFO_PRIVATE_LIB="${THIRDPARTY_WASM_ROOT}/podofo_noopenssl_xml2_iconv/src/podofo/private/libpodofo_private.a"
PODOFO_3RDPARTY_LIB="${THIRDPARTY_WASM_ROOT}/podofo_noopenssl_xml2_iconv/3rdparty/libpodofo_3rdparty.a"
LIBXML2_LIB="${THIRDPARTY_WASM_ROOT}/libxml2-install-iconv/lib/libxml2.a"
FREETYPE_LIB="${THIRDPARTY_WASM_ROOT}/freetype2/libfreetype.a"
ZLIB_LIB="${THIRDPARTY_WASM_ROOT}/zlib/libz.a"
ICONV_LIB="${THIRDPARTY_WASM_ROOT}/libiconv-install/lib/libiconv.a"
MINIZIP_LIB="${THIRDPARTY_WASM_ROOT}/minizip-ng/libminizip.a"
TINYXML2_LIB="${THIRDPARTY_WASM_ROOT}/tinyxml2/libtinyxml2.a"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing command in PATH: $1" >&2
    exit 1
  fi
}

require_file() {
  if [[ ! -f "$1" ]]; then
    echo "Missing required file: $1" >&2
    exit 1
  fi
}

require_cmd em++

mkdir -p "${OUT_DIR}"

if [[ "${USE_CCACHE}" != "off" ]] && command -v ccache >/dev/null 2>&1; then
  mkdir -p "${CCACHE_DIR_PATH}"
  export CCACHE_DIR="${CCACHE_DIR_PATH}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${PROJECT_ROOT}}"
  export CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
  export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
  export EM_COMPILER_WRAPPER="${EM_COMPILER_WRAPPER:-$(command -v ccache)}"
  echo "[build_wasm_stub] ccache: enabled (${EM_COMPILER_WRAPPER})"
elif [[ "${USE_CCACHE}" == "on" ]]; then
  echo "USE_CCACHE=on but ccache is not available in PATH" >&2
  exit 1
else
  echo "[build_wasm_stub] ccache: disabled"
fi

require_file "${PODOFO_LIB}"
require_file "${PODOFO_PRIVATE_LIB}"
require_file "${PODOFO_3RDPARTY_LIB}"
require_file "${LIBXML2_LIB}"
require_file "${FREETYPE_LIB}"
require_file "${ZLIB_LIB}"
require_file "${ICONV_LIB}"
require_file "${MINIZIP_LIB}"
require_file "${TINYXML2_LIB}"

CXXFLAGS_COMMON=(
  "-std=c++20"
  "-fexceptions"
  "-Wall"
  "-Wextra"
  "-Wno-unused-parameter"
  "-I${CPP_ROOT}/include"
  "-I${CPP_ROOT}/src/legacy_pdf2docx"
  "-I${PROJECT_ROOT}/thirdparty/podofo/src"
  "-I${THIRDPARTY_WASM_ROOT}/podofo_noopenssl_xml2_iconv/src/podofo"
  "-I${THIRDPARTY_WASM_ROOT}/podofo_noopenssl_xml2_iconv/src/podofo/podofo/auxiliary"
  "-I${THIRDPARTY_WASM_ROOT}/libxml2-install-iconv/include/libxml2"
  "-I${PROJECT_ROOT}/thirdparty/freetype2/include"
  "-I${THIRDPARTY_WASM_ROOT}/freetype2/include"
  "-I${PROJECT_ROOT}/thirdparty/tinyxml2"
  "-I${PROJECT_ROOT}/thirdparty/minizip-ng"
  "-I${PROJECT_ROOT}/thirdparty/minizip-ng/compat"
  "-I${PROJECT_ROOT}/thirdparty/zlib"
  "-I${THIRDPARTY_WASM_ROOT}/libiconv-install/include"
  "-DPDF2DOCX_XML_BACKEND_STR=\"tinyxml2\""
  "-DPDF2DOCX_PDF_BACKEND_STR=\"podofo\""
  "-DPDF2DOCX_HAS_ZLIB=1"
  "-DPDF2DOCX_HAS_TINYXML2=1"
  "-DPDF2DOCX_HAS_MINIZIP=1"
  "-DPDF2DOCX_HAS_FREETYPE=1"
  "-DPDF2DOCX_HAS_PODOFO=1"
  "-sWASM=1"
  "-sUSE_PTHREADS=0"
  "-sFILESYSTEM=1"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sDISABLE_EXCEPTION_CATCHING=0"
  "-sNO_EXIT_RUNTIME=1"
  "-sMODULARIZE=1"
  "-sEXPORT_ES6=1"
  "-sENVIRONMENT=web,worker"
  "-sEXPORT_NAME=createPdftoolsWasmModule"
  "-sEXPORTED_FUNCTIONS=[\"_pdftools_wasm_op\",\"_pdftools_wasm_free\",\"_malloc\",\"_free\"]"
  "-sEXPORTED_RUNTIME_METHODS=[\"FS\"]"
)

if [[ "${BUILD_TYPE}" == "Debug" ]]; then
  OPT_FLAGS=("-O0" "-g3")
else
  OPT_FLAGS=("-O3")
fi

echo "[build_wasm_stub] output: ${OUT_JS}"
echo "[build_wasm_stub] build type: ${BUILD_TYPE}"

em++ \
  "${CPP_ROOT}/src/wasm/pdftools_wasm_capi.cpp" \
  "${CPP_ROOT}/src/core/status.cpp" \
  "${CPP_ROOT}/src/core/error_handling.cpp" \
  "${CPP_ROOT}/src/pdf/document_ops.cpp" \
  "${CPP_ROOT}/src/convert/pdf2docx/pdf2docx.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/core/status.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/core/ir_html.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/core/ir_json.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/api/converter.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/backend/podofo/podofo_backend.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/font/freetype_probe.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/font/font_resolver.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/pipeline/pipeline.cpp" \
  "${CPP_ROOT}/src/legacy_pdf2docx/docx/p0_writer.cpp" \
  "-Wl,--start-group" \
  "${PODOFO_LIB}" \
  "${PODOFO_PRIVATE_LIB}" \
  "${PODOFO_3RDPARTY_LIB}" \
  "${LIBXML2_LIB}" \
  "${FREETYPE_LIB}" \
  "${ZLIB_LIB}" \
  "${ICONV_LIB}" \
  "${MINIZIP_LIB}" \
  "${TINYXML2_LIB}" \
  "-Wl,--end-group" \
  "${CXXFLAGS_COMMON[@]}" \
  "${OPT_FLAGS[@]}" \
  -o "${OUT_JS}"

echo "[build_wasm_stub] done"
ls -lh "${OUT_DIR}/pdftools_wasm.mjs" "${OUT_DIR}/pdftools_wasm.wasm"
