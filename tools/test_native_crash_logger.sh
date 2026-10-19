#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

# The probe terminates child processes with SIGSEGV; keep it separate from ASan.
"${CXX:-c++}" -std=c++17 -Wall -Wextra \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/tools/test_native_crash_logger.cpp" \
  "$ROOT/app/src/main/cpp/native_crash_logger.cpp" \
  -o "$BUILD_DIR/test_native_crash_logger"
timeout 30 "$BUILD_DIR/test_native_crash_logger" "$BUILD_DIR/crash-logs"
