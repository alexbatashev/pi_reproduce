#include "common.hpp"

#include <cstdlib>
#include <dlfcn.h>
#include <exception>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

void record(const options &opts) {
  const auto &args = opts.args();

  const char *cArgs[args.size() + 2];

  int i = 0;
  cArgs[i++] = opts.input().c_str();
  for (auto &arg : args) {
    cArgs[i++] = arg.c_str();
  }
  cArgs[i] = nullptr;

  std::string outPath = std::string("PI_REPRODUCE_TRACE_PATH=") + opts.output().c_str();
  std::string ldLibraryPath = "LD_LIBRARY_PATH=" + std::string(std::getenv("LD_LIBRARY_PATH"));
  std::cerr << ldLibraryPath << "\n";
  const char * const env[] = {
    "XPTI_TRACE_ENABLE=1",
    "XPTI_FRAMEWORK_DISPATCHER=libxptifw.so",
    ldLibraryPath.c_str(),
    "XPTI_SUBSCRIBERS=libplugin_record.so",
    outPath.c_str(),
    nullptr
  };
  auto err = execve(opts.input().c_str(), const_cast<char *const *>(cArgs), const_cast<char* const*>(env));
  if (err) {
    std::cerr << "Unexpected error while running executable: " << errno << "\n";
  }
}
