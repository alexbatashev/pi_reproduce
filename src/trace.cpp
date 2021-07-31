#include "trace.hpp"

#include <errno.h>
#include <stdexcept>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

void traceMe() { ptrace(PTRACE_TRACEME, 0, 0, 0); }

Tracer::Tracer(pid p) : mPid(p) {}

static std::string readString(pid pid, std::uintptr_t addr) {
  constexpr size_t longSize = sizeof(long);

  union ptraceData {
    long data;
    char bytes[longSize];
  };

  bool isComplete = false;
  size_t readBytes = 0;

  std::string result;

  while (!isComplete) {
    ptraceData data{};
    data.data = ptrace(PTRACE_PEEKDATA, pid.get_native(),
                       reinterpret_cast<void *>(addr + readBytes), nullptr);
    size_t oldSize = result.size();
    result.resize(oldSize + longSize);

    for (size_t i = 0; i < longSize; i++) {
      result[oldSize + i] = data.bytes[i];
      if (data.bytes[i] == '\0') {
        isComplete = true;
        result.resize(oldSize + i);
        break;
      }
    }

    readBytes += longSize;
  }

  return result;
}

void Tracer::run() {
  mPid.wait();
  ptrace(PTRACE_SETOPTIONS, mPid.get_native(), 0, PTRACE_O_EXITKILL);

  while (true) {
    if (ptrace(PTRACE_SYSCALL, mPid.get_native(), 0, 0) == -1)
      throw std::runtime_error(strerror(errno));
    if (waitpid(mPid.get_native(), 0, 0) == -1)
      throw std::runtime_error(strerror(errno));

    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, mPid.get_native(), 0, &regs) == -1)
      throw std::runtime_error(strerror(errno));

    const long syscall = regs.orig_rax;

    switch (syscall) {
    case SYS_openat: {
      std::string filename = readString(mPid, regs.rsi);
      mFileOpenHandler(filename);
      break;
    }
    default:
      break;
    }

    if (ptrace(PTRACE_SYSCALL, mPid.get_native(), 0, 0) == -1)
      throw std::runtime_error(strerror(errno));
    if (waitpid(mPid.get_native(), 0, 0) == -1)
      throw std::runtime_error(strerror(errno));
    if (ptrace(PTRACE_GETREGS, mPid.get_native(), 0, &regs) == -1) {
      if (errno == ESRCH)
        return;
      throw std::runtime_error(strerror(errno));
    }
  }
}
