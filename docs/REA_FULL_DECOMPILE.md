# `libsand.so`: ARM64 reverse-engineering status

## Target and source of truth

The requested target is ARM64. The working reference is `docs/lib/arm64-v8a/libsand.so.c`, checked against `docs/lib/arm64-v8a/libsand.so` (AArch64 ELF). Do not use x86_64 addresses or calling conventions for the ARM64 implementation; `docs/lib/x86_64/` is retained only as a cross-check.

## Recovered Hex-Rays failures

The ARM64 Hex-Rays output had two failures in allocator wrappers:

- `0x16C00`: normal path calls `_Znwm` (`operator new(size)`) and returns.
- `0x16C3C`: exported `_Znam` (`operator new[]`) normal path calls `_Znwm` and returns.

Both normal paths have been written as clean C++ wrappers. The associated cold exception paths were compiler-generated; rebuilding them requires correct C++ ABI/runtime integration. The original binary is unchanged.

## What remains before this is a buildable 1:1 source port

The decompiler output is still not a clean source project. It relies on IDA/Hex-Rays types and helpers (`defs.h`), includes guessed and incomplete global declarations, ABI/runtime support routines, and contains architecture-specific data layouts. Removing the two `#error` directives only recovers those small wrappers; it does **not** prove the rest compiles or behaves identically.

The APK currently targets `arm64-v8a`. The app-side `app/src/main/cpp/sand_engine.{h,cpp}` is still a provisional simplified engine and has not yet been replaced by the decompiled implementation. A clean port must preserve original JNI signatures, global-state layout, command queue behavior, particle storage, simulation/update order, rendering, and lifecycle. Until the source builds and is behaviorally checked against the original ARM64 `.so`, it must not be described as 1:1.

## Architecture artifacts

- Target ELF: `docs/lib/arm64-v8a/libsand.so`
- Target decompilation: `docs/lib/arm64-v8a/libsand.so.c`
- Cross-reference only: `docs/lib/x86_64/libsand.so` and `.c`
