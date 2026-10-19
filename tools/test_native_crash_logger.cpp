#include "../app/src/main/cpp/native_crash_logger.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static bool run_crash_case(const char *preferred, const char *fallback,
                           const char *expected_directory) {
  const pid_t child = fork();
  if (child < 0) {
    std::perror("fork");
    return false;
  }
  if (child == 0) {
    if (sand_configure_crash_logging(preferred, fallback) == nullptr)
      _exit(90);
    sand_log_engine_loop_exit(7, 1234);
    sand_report_engine_progress(7, 1234);
    usleep(20000);
    sand_check_engine_progress();
    raise(SIGSEGV);
    _exit(91);
  }

  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
      WTERMSIG(status) != SIGSEGV) {
    std::fprintf(stderr, "crash child did not terminate from SIGSEGV (status=%d)\n", status);
    return false;
  }

  const std::string prefix = std::string(expected_directory) + "/native_crash_";
  const std::string command = "find '" + std::string(expected_directory) +
      "' -maxdepth 1 -type f -name 'native_crash_*.log' -print -quit";
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr)
    return false;
  char path[2048]{};
  const bool found = std::fgets(path, sizeof(path), pipe) != nullptr;
  const int pipe_status = pclose(pipe);
  if (!found || pipe_status != 0) {
    std::fprintf(stderr, "no native crash report found in %s\n", expected_directory);
    return false;
  }
  path[std::strcspn(path, "\r\n")] = '\0';
  if (std::strncmp(path, prefix.c_str(), prefix.size()) != 0) {
    std::fprintf(stderr, "crash report was written to an unexpected path: %s\n", path);
    return false;
  }

  std::ifstream report(path);
  const std::string contents((std::istreambuf_iterator<char>(report)),
                             std::istreambuf_iterator<char>());
  for (const char *expected : {"NATIVE ENGINE LOOP RETURNED", "engine_mode=7",
                               "simulation_ticks=1234", "PROGRESS STALLED",
                               "stalled_for_ms=", "NATIVE FATAL SIGNAL",
                               "signal_name=SIGSEGV", "signal=11", "fault_address=",
                               "crash_time_epoch_seconds=", "pc=0x", "sp=0x"}) {
    if (contents.find(expected) == std::string::npos) {
      std::fprintf(stderr, "crash report is missing '%s'\n", expected);
      return false;
    }
  }
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: test_native_crash_logger <temporary-directory>\n");
    return 2;
  }
  const rlimit no_core_dumps = {0, 0};
  setrlimit(RLIMIT_CORE, &no_core_dumps);

  const std::string root = argv[1];
  const std::string primary = root + "/public-sand";
  const std::string fallback = root + "/app-private";
  const std::string inaccessible = "/proc/sand-crash-logger-test";

  if (!run_crash_case(primary.c_str(), fallback.c_str(), primary.c_str()))
    return 1;
  if (!run_crash_case(inaccessible.c_str(), fallback.c_str(), fallback.c_str()))
    return 1;

  std::puts("native fatal-signal logger wrote primary and fallback reports");
  return 0;
}
