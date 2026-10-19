#include "native_crash_logger.h"

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
constexpr int kCrashSignals[] = {
    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP, SIGSYS, SIGXCPU, SIGXFSZ,
    SIGPIPE};

volatile sig_atomic_t g_crash_log_fd = -1;
volatile sig_atomic_t g_inside_crash_handler = 0;
bool g_handlers_installed = false;
struct sigaction g_previous_actions[NSIG]{};
char g_selected_directory[kPathCapacity]{};
thread_local void *g_alternate_stack_memory = nullptr;

void write_all(int fd, const char *data, size_t size) {
  while (size != 0) {
    const ssize_t written = write(fd, data, size);
    if (written <= 0)
      return;
    data += written;
    size -= static_cast<size_t>(written);
  }
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
  write_all(fd, "\n", 1);
}

void write_signed_number(int fd, const char *label, int value) {
  write_all(fd, label, safe_c_string_length(label));
  if (value < 0) {
    write_all(fd, "-", 1);
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
  write_all(fd, "\n", 1);
}

void write_hex(int fd, const char *label, uintptr_t value) {
  write_all(fd, label, safe_c_string_length(label));
  write_all(fd, "0x", 2);
  char number[2 * sizeof(value)];
  const size_t length = append_unsigned(number, sizeof(number), value, 16);
  write_all(fd, number, length);
  write_all(fd, "\n", 1);
}

const char *signal_name(int signal_number) {
  switch (signal_number) {
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGSEGV: return "SIGSEGV";
    case SIGTRAP: return "SIGTRAP";
    case SIGSYS: return "SIGSYS";
    case SIGXCPU: return "SIGXCPU";
    case SIGXFSZ: return "SIGXFSZ";
    case SIGPIPE: return "SIGPIPE";
    default: return "UNKNOWN";
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
    write_all(fd, "=== NATIVE FATAL SIGNAL ===\n", 28);
    write_all(fd, "signal_name=", 12);
    const char *name = signal_name(signal_number);
    write_all(fd, name, safe_c_string_length(name));
    write_all(fd, "\n", 1);
    write_number(fd, "signal=", static_cast<uint64_t>(signal_number));
    if (signal_info != nullptr) {
      write_signed_number(fd, "signal_code=", signal_info->si_code);
      if (signal_info->si_code > 0) {
        write_hex(fd, "fault_address=",
                  reinterpret_cast<uintptr_t>(signal_info->si_addr));
      } else {
        write_all(fd, "fault_address=not-applicable\n", 29);
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
    write_all(fd, "=== END NATIVE FATAL SIGNAL ===\n", 32);
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
  const char armed_message[] =
      "Native crash logging armed. Fatal signal records include signal info and PC/LR/SP.\n";
  write_all(new_fd, armed_message, sizeof(armed_message) - 1);
  fsync(new_fd);
  install_handlers();
  return g_selected_directory;
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
#endif
