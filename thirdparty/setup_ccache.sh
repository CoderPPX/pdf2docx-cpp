#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/build_wasm"

CCACHE_DIR_PATH="${CCACHE_DIR:-${BUILD_ROOT}/.ccache}"
CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-20G}"

if ! command -v ccache >/dev/null 2>&1; then
  echo "ccache not found in PATH." >&2
  echo "Install it first, e.g. on Ubuntu: sudo apt-get install -y ccache" >&2
  exit 1
fi

mkdir -p "${CCACHE_DIR_PATH}"
export CCACHE_DIR="${CCACHE_DIR_PATH}"
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${PROJECT_ROOT}}"
export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
export CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
export EM_COMPILER_WRAPPER="ccache"

ccache --set-config=max_size="${CCACHE_MAXSIZE}" || true
ccache --set-config=base_dir="${CCACHE_BASEDIR}" || true
ccache --set-config=compiler_check="${CCACHE_COMPILERCHECK}" || true
ccache --set-config=hash_dir=false || true

echo "ccache configured:"
echo "  CCACHE_DIR=${CCACHE_DIR}"
echo "  CCACHE_BASEDIR=${CCACHE_BASEDIR}"
echo "  CCACHE_MAXSIZE=${CCACHE_MAXSIZE}"
echo "  EM_COMPILER_WRAPPER=${EM_COMPILER_WRAPPER}"
echo
ccache -s || true
