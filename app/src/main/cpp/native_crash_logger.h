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

// Record a normal native engine-loop return, which is not a fatal signal.
void sand_log_engine_loop_exit(int engine_mode, unsigned int simulation_ticks);
void sand_report_engine_progress(int engine_mode, unsigned int simulation_ticks);
void sand_check_engine_progress(void);

#ifdef __cplusplus
}
#endif
