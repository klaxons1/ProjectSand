#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
# Keep AddressSanitizer enabled: the frame scheduler and tool dispatch cover
# recovered stack buffers and decompiler-derived switch paths.
SANITIZER_FLAGS=(-fsanitize=address -fno-omit-frame-pointer)

"${CXX:-c++}" -std=c++17 -w \
  "${SANITIZER_FLAGS[@]}" \
  -ffunction-sections -fdata-sections \
  -I"$ROOT/tools/host_engine_stubs" \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/tools/host_engine_motion_probe.cpp" \
  -Wl,--gc-sections -o "$BUILD_DIR/host_engine_motion_probe"
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" "$BUILD_DIR/host_engine_motion_probe"

"${CXX:-c++}" -std=c++17 -w \
  "${SANITIZER_FLAGS[@]}" \
  -ffunction-sections -fdata-sections \
  -I"$ROOT/tools/host_engine_stubs" \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/tools/host_engine_frame_probe.cpp" \
  -Wl,--gc-sections -o "$BUILD_DIR/host_engine_frame_probe"
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" "$BUILD_DIR/host_engine_frame_probe"
