#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Installs process-wide fatal-signal logging and returns the directory that
// successfully opened a log file (the preferred directory or the fallback).
const char *sand_configure_crash_logging(const char *preferred_directory,
                                         const char *fallback_directory);

// Register an alternate signal stack for the calling thread. Signal stacks are
// per-thread, so every Java/native thread that may run engine code should call it.
void sand_prepare_crash_signal_stack(void);

#ifdef __cplusplus
}
#endif
