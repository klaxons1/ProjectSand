# Полный декомпил libsand.so — REA via Hex-Rays 9.4 + rea-agents objdump

## Источник
- `docs/lib/x86_64/libsand.so` sha256 `670b355a1fcef6022479342af6c957792fc41294459677cc4b7b925a4fb23660` (elf64 x86_64, 136K) — REA `inspect-with-objdump disassemble` + `inspect-binary-layout` + Hex-Rays decompile 17 287 строк → `docs/lib/x86_64/libsand.so.c`
- `docs/lib/arm64-v8a/libsand.so` (elf aarch64, 143K) — 18 006 строк → `docs/lib/arm64-v8a/libsand.so.c`
- Сигнатуры: `Java_smellymoo_sand_Engine_init` x86 `0x15100` / arm64 `0x16778`, `run_command 0x140D0/0x169a0`, `main_loop 0x14620/0x16f10`, `init_all 0x13DD0/0x16510` — `readelf -Ws` + `nm -D`

## Что декомпилировано
- `init_all(char a1)` — вся таблица частиц `qword_9ABE80[150003*idx]`, `byte_22058` групп, `xmmword_225E0/225F0/22600` состояние, `chain_update/clear_screen/clear_eraser`
- `run_command(command_struct)` — 24 ветки switch (1 KILL/2 DOWN/3 MOVE/4 UP/5 SET_MAT/7 slowMo/8 borders/9-10 zoom/11 init_all/12 clear/13 spouts/14 release/15 line/16 background/17 random type/18 pause/19 view/20 brushSize/22 restore+calc_state_all/23 snapshot/24 wipe)
- `main_loop()` — ANativeWindow lock loop, очередь 50 команд `unk_9ABD40`, `spouts texture()→add_type()`, particle scan `current_can_sleep/seek_particle/seek_gas/seek_boom/seek_ray/pressures`, `set_frame + ANativeWindow_unlockAndPost + usleep 100ms`, состояния `byte_2204B`
- 40+ физик-функций: `slide_powder/liquid`, `pull_surface_HRib`, `drop_interupted`, `process_transistion`, `wake_around`, `shift_drop/lines`, `draw_point/square/particle`, `texture`, `chain_append` — в `.c` с адресами

## Как перенесено в свой cpp
`app/src/main/cpp/sand_engine.{h,cpp}` arm64-only, CMake 3.22.1 — все 60 mangled `_Z...` объявлены как обёртки на `init_simulation/simulate_step/render_frame/process_command/main_loop/init_all/texture/change_zoom`. Полная 1:1 физика требует Ghidra типов, сейчас MVP рисует и собирает CI; декомпил лежит как референс для добивки.

## REA-трасса
`npx rea-agents inspect-with-objdump libsand.so disassemble --format json` → evidence `ev_29b64de7...`, objdump .plt/.text, JSON-верификация.
