
# ProjectSand — Rebuildable Port via REA

Reconstructed from APK `smellymoo.sand` v86 (API 10–21) using [morluto/rea](https://github.com/morluto/rea).

## Stack
- Gradle 8.5 / AGP 8.2.2 / SDK 34 / NDK r27 (27.0.11718014)
- CMake 3.22.1 + `liblog` / `libandroid`
- Native rebuild is **not yet 1:1**: `app/src/main/cpp/sand_engine.{h,cpp}` is a provisional simplified simulation. The ARM64 Hex-Rays output in `docs/lib/arm64-v8a/libsand.so.c` is the source-reconstruction reference; it is not yet integrated into the APK build. See `docs/REA_FULL_DECOMPILE.md` for status and limitations.
- Java sources in `app/src/main/java/smellymoo/sand/` (11 files + billing service) decompiled via REA → JADX, normalized (`C0000R` → `R`), `BuildConfig` omitted (generated)
- Resources decoded via `androguard`/`aapt2`: `AndroidManifest.xml`, `res/values/` (strings/colors/styles/ids), `res/layout` (13), `res/drawable-*`, `assets/help*.html`

## Build
```sh
./gradlew assembleDebug
./gradlew assembleRelease   # also run by CI
```

## CI
`.github/workflows/android.yml` — triggers on `arena/*`, installs NDK 27.0.11718014 via `setup-android@v3`, builds both variants, uploads APK artifacts.

## Reverse Engineering Notes
- `docs/` holds original decoded artifacts for reference (not packaged)
- `docs/README.md` in REA repo documents `npx rea-agents` workflows used for extraction
