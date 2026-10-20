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
# Replay the complete fire/gunpowder/firework scheduler scenario with varied
# random states; the reported failure was intermittent and seed-sensitive.
for seed in 1 2 3 4 5 6 7 8; do
  ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" \
    "$BUILD_DIR/host_engine_frame_probe" "$seed"
done

"${CXX:-c++}" -std=c++17 -w \
  "${SANITIZER_FLAGS[@]}" \
  -ffunction-sections -fdata-sections \
  -I"$ROOT/tools/host_engine_stubs" \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/tools/host_engine_firework_probe.cpp" \
  -Wl,--gc-sections -o "$BUILD_DIR/host_engine_firework_probe"
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" "$BUILD_DIR/host_engine_firework_probe"
