#include "native_crash_logger.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace {

constexpr size_t kPathCapacity = 1024;
constexpr size_t kAlternateStackSize = 64 * 1024;
constexpr unsigned int kProgressReportIntervalTicks = 16;
constexpr int kSimulationMode = 7;
constexpr int kSimulationTransitionMode = 8;
constexpr uint64_t kNanosecondsPerSecond = 1000000000ULL;
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ULL;
// Test builds shorten this threshold to exercise the watchdog quickly.
#ifndef SAND_ENGINE_STALL_THRESHOLD_NS
#define SAND_ENGINE_STALL_THRESHOLD_NS (10ULL * kNanosecondsPerSecond)
#endif
constexpr uint64_t kStallThresholdNs = SAND_ENGINE_STALL_THRESHOLD_NS;

constexpr int kCrashSignals[] = {
    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP, SIGSYS, SIGXCPU, SIGXFSZ,
    SIGPIPE};

volatile sig_atomic_t g_crash_log_fd = -1;
volatile sig_atomic_t g_inside_crash_handler = 0;
bool g_handlers_installed = false;
struct sigaction g_previous_actions[NSIG]{};
char g_selected_directory[kPathCapacity]{};
// Published by the engine thread; the Java watchdog reads these atomically.
std::atomic<int> g_progress_mode{-1};
std::atomic<unsigned int> g_progress_tick{0};
std::atomic<uint64_t> g_progress_time_ns{0};
std::atomic<bool> g_stall_reported{false};
thread_local void *g_alternate_stack_memory = nullptr;

// Keep this writer allocation-free and syscall-only for signal-handler use.
void write_all(int fd, const char *data, size_t size) {
  while (size != 0) {
    const ssize_t written = write(fd, data, size);
    if (written <= 0)
      return;
    data += written;
    size -= static_cast<size_t>(written);
  }
}

template <size_t N>
void write_literal(int fd, const char (&text)[N]) {
  static_assert(N > 0, "string literals include a trailing null byte");
  write_all(fd, text, N - 1);
}

size_t safe_c_string_length(const char *text) {
  size_t length = 0;
  while (text[length] != '\0')
    ++length;
  return length;
}

size_t append_unsigned(char *buffer, size_t capacity, uint64_t value, unsigned base) {
  static constexpr char digits[] = "0123456789abcdef";
  char reversed[32];
  size_t count = 0;
  do {
    reversed[count++] = digits[value % base];
    value /= base;
  } while (value != 0 && count < sizeof(reversed));

  size_t output = 0;
  while (count != 0 && output < capacity)
    buffer[output++] = reversed[--count];
  return output;
}

void write_number(int fd, const char *label, uint64_t value) {
  write_all(fd, label, safe_c_string_length(label));
  char number[32];
  const size_t length = append_unsigned(number, sizeof(number), value, 10);
  write_all(fd, number, length);
  write_literal(fd, "\n");
}

void write_signed_number(int fd, const char *label, int value) {
  write_all(fd, label, safe_c_string_length(label));
  if (value < 0) {
    write_literal(fd, "-");
    const uint64_t magnitude = static_cast<uint64_t>(-(static_cast<int64_t>(value)));
    char number[32];
    const size_t length = append_unsigned(number, sizeof(number), magnitude, 10);
    write_all(fd, number, length);
  } else {
    char number[32];
    const size_t length = append_unsigned(number, sizeof(number),
                                          static_cast<uint64_t>(value), 10);
    write_all(fd, number, length);
  }
  write_literal(fd, "\n");
}

void write_hex(int fd, const char *label, uintptr_t value) {
  write_all(fd, label, safe_c_string_length(label));
  write_literal(fd, "0x");
  char number[2 * sizeof(value)];
  const size_t length = append_unsigned(number, sizeof(number), value, 16);
  write_all(fd, number, length);
  write_literal(fd, "\n");
}

void write_event_metadata(int fd) {
  timespec event_time{};
  if (clock_gettime(CLOCK_REALTIME, &event_time) == 0) {
    write_number(fd, "event_time_epoch_seconds=",
                 static_cast<uint64_t>(event_time.tv_sec));
    write_number(fd, "event_time_nanoseconds=",
                 static_cast<uint64_t>(event_time.tv_nsec));
  }
  write_number(fd, "pid=", static_cast<uint64_t>(getpid()));
}

const char *signal_name(int signal_number) {
  switch (signal_number) {
    case SIGABRT:
      return "SIGABRT";
    case SIGBUS:
      return "SIGBUS";
    case SIGFPE:
      return "SIGFPE";
    case SIGILL:
      return "SIGILL";
    case SIGSEGV:
      return "SIGSEGV";
    case SIGTRAP:
      return "SIGTRAP";
    case SIGSYS:
      return "SIGSYS";
    case SIGXCPU:
      return "SIGXCPU";
    case SIGXFSZ:
      return "SIGXFSZ";
    case SIGPIPE:
      return "SIGPIPE";
    default:
      return "UNKNOWN";
  }
}

void get_fault_registers(void *opaque_context, uintptr_t *pc, uintptr_t *lr,
                         uintptr_t *sp) {
  *pc = 0;
  *lr = 0;
  *sp = 0;
  if (opaque_context == nullptr)
    return;

  const auto *context = static_cast<const ucontext_t *>(opaque_context);
#if defined(__aarch64__)
  *pc = static_cast<uintptr_t>(context->uc_mcontext.pc);
  *lr = static_cast<uintptr_t>(context->uc_mcontext.regs[30]);
  *sp = static_cast<uintptr_t>(context->uc_mcontext.sp);
#elif defined(__x86_64__) && defined(REG_RIP) && defined(REG_RSP)
  *pc = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
  *sp = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
  *lr = 0;
#endif
}

void crash_signal_handler(int signal_number, siginfo_t *signal_info,
                          void *opaque_context) {
  if (g_inside_crash_handler != 0)
    _exit(128 + signal_number);
  g_inside_crash_handler = 1;

  const int fd = g_crash_log_fd;
  if (fd >= 0) {
    write_literal(fd, "=== NATIVE FATAL SIGNAL ===\n");
    write_literal(fd, "signal_name=");
    const char *name = signal_name(signal_number);
    write_all(fd, name, safe_c_string_length(name));
    write_literal(fd, "\n");
    write_number(fd, "signal=", static_cast<uint64_t>(signal_number));
    if (signal_info != nullptr) {
      write_signed_number(fd, "signal_code=", signal_info->si_code);
      if (signal_info->si_code > 0) {
        write_hex(fd, "fault_address=",
                  reinterpret_cast<uintptr_t>(signal_info->si_addr));
      } else {
        write_literal(fd, "fault_address=not-applicable\n");
      }
    }
    timespec crash_time{};
    if (clock_gettime(CLOCK_REALTIME, &crash_time) == 0) {
      write_number(fd, "crash_time_epoch_seconds=",
                   static_cast<uint64_t>(crash_time.tv_sec));
      write_number(fd, "crash_time_nanoseconds=",
                   static_cast<uint64_t>(crash_time.tv_nsec));
    }
    write_number(fd, "pid=", static_cast<uint64_t>(getpid()));

    uintptr_t pc = 0;
    uintptr_t lr = 0;
    uintptr_t sp = 0;
    get_fault_registers(opaque_context, &pc, &lr, &sp);
    write_hex(fd, "pc=", pc);
    write_hex(fd, "lr=", lr);
    write_hex(fd, "sp=", sp);
    write_literal(fd, "=== END NATIVE FATAL SIGNAL ===\n");
    fsync(fd);
  }

  // Restore the platform's original handler and queue the signal again. It is
  // blocked while this handler runs; returning lets Android's debuggerd (or the
  // default action) receive it and generate the standard tombstone as well.
  if (signal_number > 0 && signal_number < NSIG) {
    const struct sigaction previous = g_previous_actions[signal_number];
    sigaction(signal_number, &previous, nullptr);
    if (previous.sa_handler == SIG_IGN)
      _exit(128 + signal_number);
  }
  if (raise(signal_number) != 0)
    _exit(128 + signal_number);
}

bool make_directories(const char *directory) {
  if (directory == nullptr || directory[0] == '\0' ||
      std::strlen(directory) >= kPathCapacity)
    return false;

  char path[kPathCapacity];
  std::strcpy(path, directory);
  for (char *cursor = path + 1; *cursor != '\0'; ++cursor) {
    if (*cursor != '/')
      continue;
    *cursor = '\0';
    if (mkdir(path, 0775) != 0 && errno != EEXIST)
      return false;
    *cursor = '/';
  }
  if (mkdir(path, 0775) != 0 && errno != EEXIST)
    return false;
  struct stat info {};
  return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

void write_executable_mappings(int fd) {
  FILE *maps = std::fopen("/proc/self/maps", "r");
  if (maps == nullptr)
    return;

  write_literal(fd, "=== EXECUTABLE MAPPINGS AT LOGGER SETUP ===\n");
  char line[2048];
  while (std::fgets(line, sizeof(line), maps) != nullptr) {
    if (std::strstr(line, " r-xp ") != nullptr ||
        std::strstr(line, " r-xs ") != nullptr) {
      write_all(fd, line, std::strlen(line));
    }
  }
  std::fclose(maps);
}

int open_crash_log(const char *directory) {
  if (!make_directories(directory))
    return -1;

  timespec now{};
  clock_gettime(CLOCK_REALTIME, &now);
  char path[kPathCapacity];
  const int length = std::snprintf(path, sizeof(path),
      "%s/native_crash_%lld_%ld_%09ld.log", directory,
      static_cast<long long>(now.tv_sec), static_cast<long>(getpid()),
      static_cast<long>(now.tv_nsec));
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(path))
    return -1;

  return open(path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0600);
}

void install_handlers() {
  if (g_handlers_installed)
    return;

  struct sigaction action {};
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = crash_signal_handler;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;

  bool installed_at_least_one = false;
  for (const int signal_number : kCrashSignals) {
    if (sigaction(signal_number, &action,
                  &g_previous_actions[signal_number]) == 0)
      installed_at_least_one = true;
  }
  g_handlers_installed = installed_at_least_one;
}

}  // namespace

extern "C" void sand_prepare_crash_signal_stack(void) {
  if (g_alternate_stack_memory != nullptr)
    return;

  void *memory = mmap(nullptr, kAlternateStackSize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED)
    return;

  stack_t alternate_stack{};
  alternate_stack.ss_sp = memory;
  alternate_stack.ss_size = kAlternateStackSize;
  alternate_stack.ss_flags = 0;
  if (sigaltstack(&alternate_stack, nullptr) != 0) {
    munmap(memory, kAlternateStackSize);
    return;
  }
  g_alternate_stack_memory = memory;
}

extern "C" const char *sand_configure_crash_logging(
    const char *preferred_directory, const char *fallback_directory) {
  sand_prepare_crash_signal_stack();

  int new_fd = open_crash_log(preferred_directory);
  const char *selected = preferred_directory;
  if (new_fd < 0) {
    new_fd = open_crash_log(fallback_directory);
    selected = fallback_directory;
  }
  if (new_fd < 0 || selected == nullptr ||
      std::strlen(selected) >= sizeof(g_selected_directory))
    return nullptr;

  // Keep previous descriptors open until process exit: a signal handler on a
  // different thread may already have loaded the old fd while this is swapped.
  g_crash_log_fd = new_fd;

  std::strcpy(g_selected_directory, selected);
  write_literal(new_fd,
                "Native crash logging armed. Fatal signal records include signal info "
                "and PC/LR/SP.\n");
  write_executable_mappings(new_fd);
  fsync(new_fd);
  install_handlers();
  return g_selected_directory;
}

extern "C" void sand_report_engine_progress(int engine_mode,
                                            unsigned int simulation_ticks) {
  const int previous_mode = g_progress_mode.load(std::memory_order_relaxed);
  const unsigned int previous_tick = g_progress_tick.load(std::memory_order_relaxed);
  if (previous_mode == engine_mode &&
      static_cast<unsigned int>(simulation_ticks - previous_tick) <
          kProgressReportIntervalTicks)
    return;

  timespec now{};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    return;
  g_progress_mode.store(engine_mode, std::memory_order_relaxed);
  g_progress_tick.store(simulation_ticks, std::memory_order_relaxed);
  g_progress_time_ns.store(static_cast<uint64_t>(now.tv_sec) *
                                   kNanosecondsPerSecond +
                               static_cast<uint64_t>(now.tv_nsec),
                           std::memory_order_release);
  g_stall_reported.store(false, std::memory_order_release);
}

extern "C" void sand_check_engine_progress(void) {
  const uint64_t last_progress = g_progress_time_ns.load(std::memory_order_acquire);
  if (last_progress == 0)
    return;
  const int mode = g_progress_mode.load(std::memory_order_relaxed);
  // These are the two active update phases in main_loop(); idle and paused
  // modes are deliberately excluded to avoid reporting normal pauses as hangs.
  if (mode != kSimulationMode && mode != kSimulationTransitionMode)
    return;

  timespec now{};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    return;
  const uint64_t now_ns = static_cast<uint64_t>(now.tv_sec) *
                              kNanosecondsPerSecond +
                          static_cast<uint64_t>(now.tv_nsec);
  if (now_ns <= last_progress || now_ns - last_progress < kStallThresholdNs)
    return;

  bool expected = false;
  if (!g_stall_reported.compare_exchange_strong(expected, true,
                                                 std::memory_order_acq_rel))
    return;
  const int fd = g_crash_log_fd;
  if (fd < 0)
    return;

  write_literal(fd, "=== NATIVE ENGINE PROGRESS STALLED ===\n");
  write_signed_number(fd, "engine_mode=", mode);
  write_number(fd, "simulation_ticks=",
               static_cast<uint64_t>(g_progress_tick.load(std::memory_order_relaxed)));
  write_number(fd, "stalled_for_ms=",
               (now_ns - last_progress) / kNanosecondsPerMillisecond);
  write_event_metadata(fd);
  fsync(fd);
}

extern "C" void sand_log_engine_loop_exit(int engine_mode,
                                          unsigned int simulation_ticks) {
  const int fd = g_crash_log_fd;
  if (fd < 0)
    return;

  write_literal(fd, "=== NATIVE ENGINE LOOP RETURNED ===\n");
  write_signed_number(fd, "engine_mode=", engine_mode);
  write_number(fd, "simulation_ticks=",
               static_cast<uint64_t>(simulation_ticks));
  write_event_metadata(fd);
  fsync(fd);
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT jstring JNICALL
Java_smellymoo_sand_Engine_configureCrashLogging(JNIEnv *environment, jclass,
                                                  jstring preferred_directory,
                                                  jstring fallback_directory) {
  const char *preferred = preferred_directory == nullptr
      ? nullptr : environment->GetStringUTFChars(preferred_directory, nullptr);
  const char *fallback = fallback_directory == nullptr
      ? nullptr : environment->GetStringUTFChars(fallback_directory, nullptr);

  const char *selected = sand_configure_crash_logging(preferred, fallback);
  jstring result = selected == nullptr ? nullptr : environment->NewStringUTF(selected);

  if (preferred != nullptr)
    environment->ReleaseStringUTFChars(preferred_directory, preferred);
  if (fallback != nullptr)
    environment->ReleaseStringUTFChars(fallback_directory, fallback);
  return result;
}

extern "C" JNIEXPORT void JNICALL
Java_smellymoo_sand_Engine_prepareCrashSignalStack(JNIEnv *, jclass) {
  sand_prepare_crash_signal_stack();
}

extern "C" JNIEXPORT void JNICALL
Java_smellymoo_sand_Engine_checkNativeEngineProgress(JNIEnv *, jclass) {
  sand_check_engine_progress();
}
#endif
