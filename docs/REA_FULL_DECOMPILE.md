# ARM64 source reconstruction status

## Target and source of truth

The requested ABI is `arm64-v8a`. The reference inputs are `docs/lib/arm64-v8a/libsand.so.c` and the matching AArch64 ELF `docs/lib/arm64-v8a/libsand.so`. The x86_64 artifacts are cross-reference material only. The original `.so` is not linked or packaged as a replacement.

## Current source path

`tools/generate_arm64_decompile.py` produces `app/src/main/cpp/arm64/libsand_decompiled.cpp`; `app/src/main/cpp/CMakeLists.txt` now builds that file as `libsand.so` (output name `sand`) and rejects non-ARM64 Android ABIs. The earlier grid-based simulation sources remain in the tree for reference but are not part of the native target.

The generator maps the ELF `.data` and `.bss` address range into one contiguous, zero-initialized native memory arena. It copies the original `.data` bytes into the start of that arena and maps the decompiler's named globals at their original offsets. This is needed because engine code indexes through the decompiler's `byte_33008` base across the `.data`/`.bss` boundary. The `.bss` range remains `0x33510`–`0xE50EF8` (size `0xE1D9E8`). A referenced read-only table is kept in the generated source. ELF startup/libc++abi teardown shims are omitted in favor of Android's runtime, the private JNI dispatch is expressed through the NDK `JNIEnv` API, and both native entry points retain unmangled `JNIEXPORT`/`JNICALL` linkage for ART lookup.

The generator also makes several decompiler pointer/integer casts explicit and preserves unsigned initializer bit patterns. These are source-normalization steps, not proof that every inferred type or operation matches the original behavior.

## Verification and remaining work

- **Passed:** generator execution; strict host `g++ -std=c++17 -fsyntax-only` using temporary host Android/JNI stubs. The current generated source reports zero syntax errors.
- **Not run here:** Android/NDK compile and link. This environment has no Android NDK toolchain or Java/JDK executable available.
- **External Gradle log received:** it linked `sand_engine.cpp.o` and `ported_decompile.cpp.o` and failed on undefined simulation globals (`g_zoom_raw`, `g_selectedMat`, etc.). That log used the old CMake source list, so it did not build this generated source. The current `CMakeLists.txt` lists only `arm64/libsand_decompiled.cpp`; do not fix that old failure by adding simulation stubs.
- **Not run:** device/emulator behavior comparison against the original ARM64 library.

Therefore the source is wired into the ARM64 CMake target, but a successful Android build and behavioral 1:1 equivalence have **not** been established. In particular, the decompiler's inferred signatures and pointer/integer conversions still need audit against the ARM64 instructions, followed by NDK build and runtime tests covering JNI lifecycle, drawing, commands, and particle evolution.

## Important artifacts

- ARM64 ELF: `docs/lib/arm64-v8a/libsand.so`
- ARM64 decompilation: `docs/lib/arm64-v8a/libsand.so.c`
- Generator: `tools/generate_arm64_decompile.py`
- Generated candidate: `app/src/main/cpp/arm64/libsand_decompiled.cpp`
- Compatibility layer: `app/src/main/cpp/defs.h`
