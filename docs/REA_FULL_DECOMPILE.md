# ARM64 source reconstruction status

## Target and source of truth

The requested ABI is `arm64-v8a`. The reference inputs are `docs/lib/arm64-v8a/libsand.so.c` and the matching AArch64 ELF `docs/lib/arm64-v8a/libsand.so`. The x86_64 artifacts are cross-reference material only. The original `.so` is not linked or packaged as a replacement.

## Current source path

`tools/generate_arm64_decompile.py` produces `app/src/main/cpp/arm64/libsand_decompiled.cpp`; `app/src/main/cpp/CMakeLists.txt` now builds that file as `libsand.so` (output name `sand`) and rejects non-ARM64 Android ABIs. The earlier grid-based simulation sources remain in the tree for reference but are not part of the native target.

The generator maps the ELF `.data` and `.bss` address range into one contiguous, zero-initialized native memory arena. It copies the original `.data` bytes into the start of that arena and maps the decompiler's named globals at their original offsets. This is needed because engine code indexes through the decompiler's `byte_33008` base across the `.data`/`.bss` boundary. The `.bss` range remains `0x33510`–`0xE50EF8` (size `0xE1D9E8`). A referenced read-only table is kept in the generated source. ELF startup/libc++abi teardown shims are omitted in favor of Android's runtime, the private JNI dispatch is expressed through the NDK `JNIEnv` API, and both native entry points retain unmangled `JNIEXPORT`/`JNICALL` linkage for ART lookup.

The generator removes the orphan PLT resolver and dead libc++abi prototypes, renames the principal address-derived engine globals to semantic names, and renames the key particle-record locals. It also makes several decompiler pointer/integer casts explicit and preserves unsigned initializer bit patterns. Array views for the shared engine data, material table, particle records, and native-window buffer are extended to the bounds actually used in the shared arena. In `draw_particle`, the invalid `dword_0 + particle_pointer` expression is rewritten as a field read at `particle_pointer + 2`, matching the ARM64 load from `[x7, #2]`. These are concrete deobfuscation/reconstruction steps, not proof that every inferred type or operation matches the original behavior.

## Recovered state names

| Generated name | ELF address | Recovered role |
|---|---:|---|
| `g_shared_engine_data` | `0x33008` | Shared header/material-state area; its first byte is also a layer/buffer count. |
| `g_material_properties` | `0x33018` | 12-byte material-property records overlaid on the shared data area. |
| `g_framebuffer_stride` | `0x3300C` | Row stride copied from the locked `ANativeWindow_Buffer`. |
| `g_engine_mode` | `0x33010` | Native engine lifecycle/render mode. |
| `g_current_particle_index` | `0x33014` | Current particle index, including the `-2` sentinel. |
| `g_view_width`, `g_view_height` | `0x33510`, `0x33512` | Simulation dimensions after zoom scaling. |
| `g_background_color` | `0x33514` | Background color used when clearing. |
| `g_particle_records` | `0x4C7380` | Ten 16-bit fields per particle. |
| `g_particle_record_aux` | `0x4C7410` | Large auxiliary particle-state byte region. |
| `g_framebuffer_pixels`, `g_cell_state_grid` | `0xE50A08`, `0xE50A10` | Current pixel buffer and per-cell simulation allocation. |
| `g_jni_env`, `g_surface_global_ref`, `g_native_window` | `0xE50A78`, `0xE50A80`, `0xE50A88` | JNI/window lifecycle handles. |
| `g_command_queue` | `0xE50B08` | 50-entry, 6-byte command ring; adjacent names are its read/write indexes. |

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
