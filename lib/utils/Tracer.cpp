#include "utils/Tracer.hpp"

#include <cstring>
#include <errno.h>
#include <iostream>
#include <linux/limits.h>
#include <stdexcept>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <syscall.h>
#include <thread>
#include <unistd.h>

static void traceMe() { ptrace(PTRACE_TRACEME, 0, 0, 0); }

static std::string readString(pid_t pid, std::uintptr_t addr) {
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
    data.data = ptrace(PTRACE_PEEKDATA, pid,
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

static void writeString(pid_t pid, std::uintptr_t addr, std::uintptr_t reg,
                        std::string_view str) {
  char *stackAddr, *fileAddr;

  stackAddr = reinterpret_cast<char *>(
      ptrace(PTRACE_PEEKUSER, pid, sizeof(long) * addr, nullptr));
  stackAddr -= 128 + PATH_MAX;

  fileAddr = stackAddr;

  bool end = false;
  size_t offset = 0;
  while (!end) {
    union pokeData {
      long data;
      char buf[sizeof(long)];
    };

    pokeData data;
    for (size_t i = 0; i < sizeof(long), i + offset <= str.size(); i++) {
      if (i + offset == str.size()) [[unlikely]] {
        data.buf[i] = '\0';
        end = true;
      } else {
        data.buf[i] = str[offset + i];
        std::cerr << str[offset + i];
      }
    }

    ptrace(PTRACE_POKEDATA, pid, stackAddr, data.data);
    stackAddr += sizeof(long);
    offset += sizeof(long);
  }
  std::cerr << "\n";

  ptrace(PTRACE_POKEUSER, pid, sizeof(long) * reg, fileAddr);
}

namespace dpcpp_trace {
namespace detail {

class OpenHandlerImpl : public OpenHandler {
public:
  OpenHandlerImpl(pid_t p, std::uintptr_t stackAddr, unsigned long fileNameReg)
      : mPid(p), mStackAddr(stackAddr), mFileNameRegister(fileNameReg) {}
  void replaceFilename(std::string_view newFile) const final {
    writeString(mPid, mStackAddr, mFileNameRegister, newFile);
  }

private:
  pid_t mPid;
  std::uintptr_t mStackAddr;
  unsigned long mFileNameRegister;
};

class StatHandlerImpl : public StatHandler {
public:
  StatHandlerImpl(pid_t p, std::uintptr_t stackAddr, unsigned long fileNameReg)
      : mPid(p), mStackAddr(stackAddr), mFileNameRegister(fileNameReg) {}
  void replaceFilename(std::string_view newFile) const final {
    writeString(mPid, mStackAddr, mFileNameRegister, newFile);
  }

private:
  pid_t mPid;
  std::uintptr_t mStackAddr;
  unsigned long mFileNameRegister;
};

class NativeTracerImpl {
public:
  NativeTracerImpl() = default;

  void launch(std::string_view executable, std::span<std::string> args,
              std::span<std::string> env) {
    const auto toCString = [](const std::string &str) { return str.c_str(); };

    std::vector<const char *> cArgs;
    cArgs.reserve(args.size() + 1);
    std::transform(args.begin(), args.end(), std::back_inserter(cArgs),
                   toCString);
    cArgs.push_back(nullptr);

    std::vector<const char *> cEnv;
    cEnv.reserve(env.size());
    std::transform(env.begin(), env.end(), std::back_inserter(cEnv), toCString);
    cEnv.push_back(nullptr);

    const auto start = [&]() {
      traceMe();
      auto err =
          execve(executable.data(), const_cast<char *const *>(cArgs.data()),
                 const_cast<char *const *>(cEnv.data()));
      if (err) {
        std::cerr << "Unexpected error while running executable: "
                  << strerror(errno) << "\n";
      }
    };

    fork(std::move(start));
  }

  void fork(std::function<void()> child) {
    pid_t pidValue = ::fork();
    if (pidValue == 0) {
      child();
      exit(0);
    } else {
      const auto mainLoop = [this, pidValue](std::stop_token token) {
        if (ptrace(PTRACE_ATTACH, pidValue, 0, 0) == -1)
          throw std::runtime_error("Failed to attach to process");
        if (waitpid(pidValue, 0, 0) == -1)
          throw std::runtime_error(strerror(errno));
        ptrace(PTRACE_SETOPTIONS, pidValue, 0, PTRACE_O_TRACESYSGOOD);
        ptrace(PTRACE_SETOPTIONS, pidValue, 0, PTRACE_O_EXITKILL);

        while (!token.stop_requested()) {
          if (ptrace(PTRACE_SYSCALL, pidValue, 0, 0) == -1)
            break;
          if (waitpid(pidValue, 0, 0) == -1)
            throw std::runtime_error(strerror(errno));

          struct user_regs_struct regs;
          if (ptrace(PTRACE_GETREGS, pidValue, 0, &regs) == -1)
            throw std::runtime_error(strerror(errno));

          const long syscall = regs.orig_rax;

          switch (syscall) {
          case SYS_stat: {
            StatHandlerImpl handler{pidValue, regs.rsp, regs.rdi};
            std::string filename = readString(pidValue, regs.rdi);
            mStatHandler(filename, handler);
            break;
          }
          case SYS_newfstatat: {
            StatHandlerImpl handler{pidValue, regs.rsp, regs.rsi};
            std::string filename = readString(pidValue, regs.rsi);
            mStatHandler(filename, handler);
            break;
          }
          case SYS_openat: {
            OpenHandlerImpl handler{pidValue, regs.rsp, regs.rsi};
            std::string filename = readString(pidValue, regs.rsi);
            mOpenFileHandler(filename, handler);
            break;
          }
          default:
            break;
          }
          if (ptrace(PTRACE_SYSCALL, pidValue, 0, 0) == -1)
            throw std::runtime_error(strerror(errno));
          if (waitpid(pidValue, 0, 0) == -1)
            throw std::runtime_error(strerror(errno));
          if (ptrace(PTRACE_GETREGS, pidValue, 0, &regs) == -1) {
            if (errno == ESRCH)
              break;
            throw std::runtime_error(strerror(errno));
          }
        }
      };

      mWorker = std::jthread(mainLoop);
    }
  }

  void onFileOpen(NativeTracer::onFileOpenHandler handler) {
    mOpenFileHandler = handler;
  }
  void onStat(NativeTracer::onStatHandler handler) { mStatHandler = handler; }

  void wait() {
    if (mWorker.joinable())
      mWorker.join();
  }

  ~NativeTracerImpl() {
    mWorker.request_stop();
    if (mWorker.joinable())
      mWorker.join();
  }

private:
  NativeTracer::onFileOpenHandler mOpenFileHandler = [](std::string_view,
                                                        const OpenHandler &) {};
  NativeTracer::onStatHandler mStatHandler = [](std::string_view,
                                                const StatHandler &) {};
  std::jthread mWorker;
};
} // namespace detail

NativeTracer::NativeTracer() {
  mImpl = std::make_shared<detail::NativeTracerImpl>();
}

void NativeTracer::launch(std::string_view executable,
                          std::span<std::string> args,
                          std::span<std::string> env) {
  mImpl->launch(executable, args, env);
}

void NativeTracer::fork(std::function<void()> child) { mImpl->fork(child); }

void NativeTracer::onFileOpen(NativeTracer::onFileOpenHandler handler) {
  mImpl->onFileOpen(handler);
}
void NativeTracer::onStat(NativeTracer::onStatHandler handler) {
  mImpl->onStat(handler);
}

void NativeTracer::wait() { mImpl->wait(); }
} // namespace dpcpp_trace
