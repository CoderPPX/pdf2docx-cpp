#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/build_wasm"
BUILD_TYPE="${BUILD_TYPE:-Release}"
THREAD_FLAGS="-sUSE_PTHREADS=0"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ccache: auto | on | off
USE_CCACHE="${USE_CCACHE:-auto}"
CCACHE_DIR_PATH="${CCACHE_DIR:-${BUILD_ROOT}/.ccache}"
CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-20G}"
CCACHE_BIN=""
CCACHE_ENABLED=0
CMAKE_CCACHE_ARGS=()
CLEAN=0

usage() {
  cat <<'EOF'
Usage: thirdparty/build_wasm_all.sh [--clean] [--ccache] [--no-ccache]

Environment variables:
  BUILD_TYPE   CMake build type (default: Release)
  JOBS         Parallel jobs (default: CPU count)
  USE_CCACHE   auto|on|off (default: auto)
  CCACHE_DIR   ccache cache directory (default: thirdparty/build_wasm/.ccache)
  CCACHE_MAXSIZE ccache max size (default: 20G)

This script builds wasm static libs for:
  zlib -> freetype2 -> libiconv -> libxml2(iconv, no threads) -> podofo(no openssl)
EOF
}

log() {
  printf '\n[%s] %s\n' "$(date '+%H:%M:%S')" "$*"
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing command: $1" >&2
    exit 1
  fi
}

parse_args() {
  for arg in "$@"; do
    case "$arg" in
      --clean)
        CLEAN=1
        ;;
      --ccache)
        USE_CCACHE="on"
        ;;
      --no-ccache)
        USE_CCACHE="off"
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        echo "Unknown argument: $arg" >&2
        usage
        exit 1
        ;;
    esac
  done
}

configure_ccache() {
  if [[ "${USE_CCACHE}" == "off" ]]; then
    log "ccache disabled (USE_CCACHE=off)"
    return
  fi

  if ! command -v ccache >/dev/null 2>&1; then
    if [[ "${USE_CCACHE}" == "on" ]]; then
      echo "USE_CCACHE=on but ccache not found in PATH" >&2
      exit 1
    fi
    log "ccache not found; continue without compiler cache"
    return
  fi

  CCACHE_BIN="$(command -v ccache)"
  CCACHE_ENABLED=1

  mkdir -p "${CCACHE_DIR_PATH}"
  export CCACHE_DIR="${CCACHE_DIR_PATH}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${PROJECT_ROOT}}"
  export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
  export CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
  export EM_COMPILER_WRAPPER="${CCACHE_BIN}"

  # Best-effort persistent config (works with modern ccache; ignored otherwise).
  "${CCACHE_BIN}" --set-config=max_size="${CCACHE_MAXSIZE}" >/dev/null 2>&1 || true
  "${CCACHE_BIN}" --set-config=base_dir="${CCACHE_BASEDIR}" >/dev/null 2>&1 || true
  "${CCACHE_BIN}" --set-config=compiler_check="${CCACHE_COMPILERCHECK}" >/dev/null 2>&1 || true
  "${CCACHE_BIN}" --set-config=hash_dir=false >/dev/null 2>&1 || true

  CMAKE_CCACHE_ARGS=(
    "-DCMAKE_C_COMPILER_LAUNCHER=${CCACHE_BIN}"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=${CCACHE_BIN}"
  )

  log "ccache enabled"
  echo "  CCACHE_DIR=${CCACHE_DIR}"
  echo "  CCACHE_BASEDIR=${CCACHE_BASEDIR}"
  echo "  CCACHE_MAXSIZE=${CCACHE_MAXSIZE}"
}

print_ccache_stats() {
  if [[ "${CCACHE_ENABLED}" -eq 0 ]]; then
    return
  fi

  log "ccache stats"
  "${CCACHE_BIN}" -s || true
}

patch_libiconv_aliases() {
  local file="${SCRIPT_DIR}/libiconv/source/lib/aliases.h"
  if [[ ! -f "$file" ]]; then
    echo "Missing file: $file" >&2
    exit 1
  fi

  if grep -q "define INT_PTR intptr_t" "$file"; then
    return
  fi

  log "Patch libiconv aliases.h for non-Windows compiler (__int32 -> intptr_t)"
  perl -0777 -i -pe 's/#ifndef INT_PTR\s+#if _WIN64\s+#define INT_PTR __int64\s+#else\s+#define INT_PTR __int32\s+#endif\s+#endif/#ifndef INT_PTR\n#if defined(_WIN64)\n#define INT_PTR __int64\n#elif defined(_WIN32)\n#define INT_PTR __int32\n#else\n#include <stdint.h>\n#define INT_PTR intptr_t\n#endif\n#endif/s' "$file"
}

build_zlib() {
  local build_dir="${BUILD_ROOT}/zlib"
  log "Build zlib -> ${build_dir}/libz.a"
  emcmake cmake -S "${SCRIPT_DIR}/zlib" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS=OFF \
    "${CMAKE_CCACHE_ARGS[@]}" \
    -DCMAKE_C_FLAGS="${THREAD_FLAGS}"
  cmake --build "$build_dir" --parallel "${JOBS}"
}

build_freetype() {
  local build_dir="${BUILD_ROOT}/freetype2"
  log "Build freetype2 -> ${build_dir}/libfreetype.a"
  emcmake cmake -S "${SCRIPT_DIR}/freetype2" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS=OFF \
    "${CMAKE_CCACHE_ARGS[@]}" \
    -DFT_DISABLE_BROTLI=ON \
    -DFT_DISABLE_BZIP2=ON \
    -DFT_DISABLE_HARFBUZZ=ON \
    -DFT_DISABLE_PNG=ON \
    -DFT_DISABLE_ZLIB=ON \
    -DCMAKE_C_FLAGS="${THREAD_FLAGS}"
  cmake --build "$build_dir" --parallel "${JOBS}"
}

build_libiconv() {
  local build_dir="${BUILD_ROOT}/libiconv"
  local install_dir="${BUILD_ROOT}/libiconv-install"
  log "Build libiconv -> ${install_dir}/lib/libiconv.a"
  mkdir -p "$build_dir"

  (
    cd "$build_dir"
    local -a configure_env=(
      "CFLAGS=${THREAD_FLAGS}"
      "CXXFLAGS=${THREAD_FLAGS}"
    )
    if [[ "${CCACHE_ENABLED}" -eq 1 ]]; then
      configure_env+=("CC=${CCACHE_BIN} emcc" "CXX=${CCACHE_BIN} em++")
    fi

    env "${configure_env[@]}" \
      emconfigure "${SCRIPT_DIR}/libiconv/source/configure" \
      --host=wasm32-unknown-emscripten \
      --prefix="${install_dir}" \
      --disable-shared \
      --enable-static \
      --disable-nls

    emmake make -j"${JOBS}"
    emmake make install
  )
}

build_libxml2() {
  local build_dir="${BUILD_ROOT}/libxml2_iconv"
  local install_dir="${BUILD_ROOT}/libxml2-install-iconv"
  local iconv_install="${BUILD_ROOT}/libiconv-install"
  log "Build libxml2(iconv on, threads off) -> ${install_dir}/lib/libxml2.a"
  emcmake cmake -S "${SCRIPT_DIR}/libxml2" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${install_dir}" \
    -DBUILD_SHARED_LIBS=OFF \
    "${CMAKE_CCACHE_ARGS[@]}" \
    -DLIBXML2_WITH_HTTP=OFF \
    -DLIBXML2_WITH_PYTHON=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF \
    -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_ZLIB=OFF \
    -DLIBXML2_WITH_MODULES=OFF \
    -DLIBXML2_WITH_LEGACY=OFF \
    -DLIBXML2_WITH_ICONV=ON \
    -DLIBXML2_WITH_THREADS=OFF \
    -DIconv_INCLUDE_DIR="${iconv_install}/include" \
    -DIconv_LIBRARY="${iconv_install}/lib/libiconv.a" \
    -DCMAKE_C_FLAGS="${THREAD_FLAGS}"
  cmake --build "$build_dir" --parallel "${JOBS}"
  cmake --install "$build_dir"
}

build_podofo() {
  local build_dir="${BUILD_ROOT}/podofo_noopenssl_xml2_iconv"
  log "Build podofo(no openssl) -> ${build_dir}/src/podofo/libpodofo.a"
  emcmake cmake -S "${SCRIPT_DIR}/podofo" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DPODOFO_BUILD_LIB_ONLY=TRUE \
    -DPODOFO_BUILD_STATIC=TRUE \
    -DPODOFO_HAVE_OPENSSL=OFF \
    "${CMAKE_CCACHE_ARGS[@]}" \
    -DZLIB_LIBRARY="${BUILD_ROOT}/zlib/libz.a" \
    -DZLIB_INCLUDE_DIR="${SCRIPT_DIR}/zlib" \
    -DFREETYPE_LIBRARY="${BUILD_ROOT}/freetype2/libfreetype.a" \
    -DFREETYPE_INCLUDE_DIRS="${SCRIPT_DIR}/freetype2/include" \
    -DLIBXML2_LIBRARY="${BUILD_ROOT}/libxml2-install-iconv/lib/libxml2.a" \
    -DLIBXML2_INCLUDE_DIR="${BUILD_ROOT}/libxml2-install-iconv/include/libxml2" \
    -DCMAKE_C_FLAGS="${THREAD_FLAGS}" \
    -DCMAKE_CXX_FLAGS="${THREAD_FLAGS}"
  cmake --build "$build_dir" --parallel "${JOBS}"
}

clean_if_requested() {
  if [[ "${CLEAN}" -eq 0 ]]; then
    return
  fi
  log "Clean old wasm build directories"
  rm -rf \
    "${BUILD_ROOT}/zlib" \
    "${BUILD_ROOT}/freetype2" \
    "${BUILD_ROOT}/libiconv" \
    "${BUILD_ROOT}/libiconv-install" \
    "${BUILD_ROOT}/libxml2_iconv" \
    "${BUILD_ROOT}/libxml2-install-iconv" \
    "${BUILD_ROOT}/podofo_noopenssl_xml2_iconv"
}

main() {
  parse_args "$@"
  require_cmd emcc
  require_cmd em++
  require_cmd emcmake
  require_cmd emconfigure
  require_cmd emmake
  require_cmd cmake
  require_cmd make
  require_cmd perl

  mkdir -p "${BUILD_ROOT}"
  configure_ccache
  clean_if_requested
  patch_libiconv_aliases

  build_zlib
  build_freetype
  build_libiconv
  build_libxml2
  build_podofo
  print_ccache_stats

  log "Done"
  cat <<EOF
Artifacts:
  ${BUILD_ROOT}/zlib/libz.a
  ${BUILD_ROOT}/freetype2/libfreetype.a
  ${BUILD_ROOT}/libiconv-install/lib/libiconv.a
  ${BUILD_ROOT}/libxml2-install-iconv/lib/libxml2.a
  ${BUILD_ROOT}/podofo_noopenssl_xml2_iconv/src/podofo/libpodofo.a
EOF
}

main "$@"
