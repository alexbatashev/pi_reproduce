#include "utils/Tracer.hpp"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <linux/limits.h>
#include <stdexcept>
#include <stop_token>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <syscall.h>
#include <thread>
#include <unistd.h>

class WaitResult {
public:
  enum class ResultType { success, signal, fail, exited };
  WaitResult(ResultType type, int code) : mType(type), mCode(code) {}

  operator bool() const noexcept { return mType == ResultType::success; }

  ResultType getType() const noexcept { return mType; }

  int getCode() const noexcept { return mCode; }

private:
  ResultType mType;
  int mCode;
};

static WaitResult wait(pid_t pid) {
  int status = 0;
  pid_t res = waitpid(pid, &status, __WALL);
  if (res == -1)
    return {WaitResult::ResultType::fail, 0};

  if (WIFEXITED(status)) {
    return {WaitResult::ResultType::exited, WEXITSTATUS(status)};
  }

  if (WIFSIGNALED(status)) {
    return {WaitResult::ResultType::signal, WTERMSIG(status)};
  }

  if (WIFSTOPPED(status)) {
    int signal = WSTOPSIG(status);
    switch (signal) {
    case SIGTERM:
    case SIGSEGV:
    case SIGINT:
    case SIGILL:
    case SIGABRT:
    case SIGFPE:
      return {WaitResult::ResultType::signal, signal};
    default:
      return {WaitResult::ResultType::success, 0};
    }
  }

  if (WIFCONTINUED(status)) {
    return {WaitResult::ResultType::success, 0};
  }

  return {WaitResult::ResultType::fail, 0};
}

static void traceMe(bool hardStop = false) { ptrace(PTRACE_TRACEME, 0, 0, 0); }

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

static void writeString(pid_t pid, std::uintptr_t reg, std::string_view str) {
  char *stackAddr, *fileAddr;

  stackAddr = reinterpret_cast<char *>(
      ptrace(PTRACE_PEEKUSER, pid, sizeof(long) * RSP, nullptr));
  stackAddr -= 128 + PATH_MAX;

  fileAddr = stackAddr;

  bool end = false;
  size_t offset = 0;
  while (!end) {
    char buf[sizeof(long)];

    for (size_t i = 0; i < sizeof(long); i++) {
      if (i + offset == str.size()) [[unlikely]] {
        buf[i] = '\0';
        end = true;
        break;
      } else {
        buf[i] = str[offset + i];
      }
    }

    if (ptrace(PTRACE_POKETEXT, pid, stackAddr, *reinterpret_cast<long *>(buf)) != 0) {
      throw std::runtime_error("failed to poke data " +
                               std::string(strerror(errno)));
    }
    stackAddr += sizeof(long);
    offset += sizeof(long);
  }

  ptrace(PTRACE_POKEUSER, pid, reg, fileAddr);
}

namespace dpcpp_trace {
namespace detail {

class OpenHandlerImpl : public OpenHandler {
public:
  OpenHandlerImpl(pid_t p, unsigned long fileNameReg)
      : mPid(p), mFileNameRegister(fileNameReg) {}
  void replaceFilename(std::string_view newFile) const final {
    writeString(mPid, mFileNameRegister, newFile);
  }

private:
  pid_t mPid;
  unsigned long mFileNameRegister;
};

class StatHandlerImpl : public StatHandler {
public:
  StatHandlerImpl(pid_t p, unsigned long fileNameReg)
      : mPid(p), mFileNameRegister(fileNameReg) {}
  void replaceFilename(std::string_view newFile) const final {
    writeString(mPid, mFileNameRegister, newFile);
  }

private:
  pid_t mPid;
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

    fork(std::move(start), true);
  }

  void fork(std::function<void()> child, bool initialSuspend) {
    pid_t pidValue = ::fork();
    if (pidValue == 0) {
      child();
      exit(0);
    } else {
      if (!initialSuspend && ptrace(PTRACE_ATTACH, pidValue, 0, 0) == -1) {
        throw std::runtime_error("Failed to attach to process");
      }
      if (auto res = ::wait(pidValue); res != true) {
        if (res.getType() == WaitResult::ResultType::fail)
          throw std::runtime_error(strerror(errno));
        else {
          mExitCode = res.getCode();
          return;
        }
      }
      mWorker = [=, this](std::stop_token) {
        ptrace(PTRACE_SETOPTIONS, pidValue, 0, PTRACE_O_TRACESYSGOOD);
        ptrace(PTRACE_SETOPTIONS, pidValue, 0, PTRACE_O_EXITKILL);

        bool needsContinue = !initialSuspend;

        while (true) {
          if (ptrace(PTRACE_SYSCALL, pidValue, 0, 0) == -1)
            break;
          if (needsContinue) {
            // ptrace(PTRACE_CONT, pidValue, 0, 0);
            needsContinue = false;
          }
          if (auto res = ::wait(pidValue); res != true) {
            if (res.getType() == WaitResult::ResultType::fail)
              throw std::runtime_error(strerror(errno));
            else {
              mExitCode = res.getCode();
              return;
            }
          }

          struct user_regs_struct regs;
          if (ptrace(PTRACE_GETREGS, pidValue, 0, &regs) == -1)
            throw std::runtime_error(strerror(errno));

          const long syscall = regs.orig_rax;

          switch (syscall) {
          case SYS_stat: {
            StatHandlerImpl handler{pidValue, sizeof(long) * RDI};
            std::string filename = readString(pidValue, regs.rdi);
            mStatHandler(filename, handler);
            break;
          }
          case SYS_newfstatat: {
            StatHandlerImpl handler{pidValue, sizeof(long) * RSI};
            std::string filename = readString(pidValue, regs.rsi);
            mStatHandler(filename, handler);
            break;
          }
          case SYS_openat: {
            OpenHandlerImpl handler{pidValue, sizeof(long) * RSI};
            std::string filename = readString(pidValue, regs.rsi);
            mOpenFileHandler(filename, handler);
            break;
          }
          default:
            break;
          }
          if (ptrace(PTRACE_SYSCALL, pidValue, 0, 0) == -1)
            throw std::runtime_error(strerror(errno));
          if (auto res = ::wait(pidValue); res != true) {
            if (res.getType() == WaitResult::ResultType::fail)
              throw std::runtime_error(strerror(errno));
            else {
              mExitCode = res.getCode();
              return;
            }
          }
          if (ptrace(PTRACE_GETREGS, pidValue, 0, &regs) == -1) {
            if (errno == ESRCH)
              break;
            throw std::runtime_error(strerror(errno));
          }
        }
      };
    }
  }

  void start() {
    std::stop_token t;
    if (mWorker)
      mWorker(t);
    // mThread = std::jthread(mWorker);
  }

  void onFileOpen(NativeTracer::onFileOpenHandler handler) {
    mOpenFileHandler = handler;
  }
  void onStat(NativeTracer::onStatHandler handler) { mStatHandler = handler; }

  int wait() {
    // if (mThread.joinable())
    //  mThread.join();
    return mExitCode;
  }
  void kill() {}
  void interrupt() {}

private:
  int mExitCode = 0;
  NativeTracer::onFileOpenHandler mOpenFileHandler = [](std::string_view,
                                                        const OpenHandler &) {};
  NativeTracer::onStatHandler mStatHandler = [](std::string_view,
                                                const StatHandler &) {};

  std::function<void(std::stop_token)> mWorker;
  std::jthread mThread;
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

void NativeTracer::fork(std::function<void()> child, bool initialSuspend) {
  mImpl->fork(child, initialSuspend);
}

void NativeTracer::onFileOpen(NativeTracer::onFileOpenHandler handler) {
  mImpl->onFileOpen(handler);
}
void NativeTracer::onStat(NativeTracer::onStatHandler handler) {
  mImpl->onStat(handler);
}

void NativeTracer::start() { return mImpl->start(); }
int NativeTracer::wait() { return mImpl->wait(); }
void NativeTracer::kill() { mImpl->kill(); }
void NativeTracer::interrupt() { mImpl->interrupt(); }

} // namespace dpcpp_trace
