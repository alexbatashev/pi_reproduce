#pragma once

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <iostream>

class pid {
public:
  using native_type = pid_t;

  pid() = default;
  explicit pid(pid_t val) : mPid(val) {}

  void wait() {
    int status;
    waitpid(mPid, &status, 0);
    // todo add error handling
  }

  ~pid() = default;

  native_type get_native() { return mPid; }

private:
  native_type mPid;
};

inline pid fork(const std::function<void()> &func) {
  pid_t pidValue = fork();
  if (pidValue == 0) {
    func();
    exit(0);
    return {};
  } else {
    return pid{pidValue};
  }
}
