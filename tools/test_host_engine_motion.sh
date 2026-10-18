#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

"${CXX:-c++}" -std=c++17 -w \
  -ffunction-sections -fdata-sections \
  -I"$ROOT/tools/host_engine_stubs" \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/tools/host_engine_motion_probe.cpp" \
  -Wl,--gc-sections -o "$BUILD_DIR/host_engine_motion_probe"
"$BUILD_DIR/host_engine_motion_probe"
