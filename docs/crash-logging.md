# Crash and simulation-stall logging

Crash logging is initialized by `Main` before the simulation surface is created. The
Java handler records uncaught exceptions; the native logger records fatal signals,
engine-loop returns, and prolonged lack of simulation progress. The handlers do not
restart or otherwise modify the simulation.

## Where reports go

The logger first tries `/storage/emulated/0/sand/`. On Android 11 and newer, that
requires the user to grant the app's all-files access. On Android 6–9 the app asks
for the legacy external-storage permission. When the preferred directory cannot be
opened (including on Android 10, where scoped storage can prevent direct access),
reports go to the app-private `files/crash-logs` directory instead. The selected
native log directory is written to Logcat under the `SandCrashLog` tag.

Native reports are appended to a process-specific `native_crash_*.log` file. The
native file also records executable `/proc/self/maps` entries at logger setup so a
PC/LR address can be associated with its loaded library and symbolized against the
matching build. Java uncaught exceptions are written to `java_crash_*.log` in the
selected directory, falling back to the same app-private directory if necessary.

## Native report markers

- `NATIVE FATAL SIGNAL`: signal, code, fault address when available, timestamp,
  process ID, and PC/LR/SP. The previous platform signal handler is restored and the
  signal is re-raised so Android can still create its normal tombstone.
- `NATIVE ENGINE LOOP RETURNED`: the engine loop returned normally, with its final
  engine mode and simulation tick count. This is an event, not necessarily a crash.
- `NATIVE ENGINE PROGRESS STALLED`: the engine was in one of its active simulation
  modes but its tick counter did not advance for ten seconds. The watchdog records
  the last mode/tick and elapsed time; it deliberately does not kill or reset the
  engine.

A lack of these markers does not prove that the simulation remained healthy: the
app may have written to its private fallback directory, or the visible symptom may
not correspond to a returned loop or a stalled active-mode tick counter.

## Host checks

```sh
bash tools/test_native_crash_logger.sh
bash tools/test_host_engine_motion.sh
```

The first test exercises the primary path, fallback path, loop-return record, stall
record, and fatal-signal record in child processes. It shortens the watchdog
threshold only for that test build; the Android build keeps the ten-second limit.
