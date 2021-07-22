#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <iostream>

class pid {
public:
  pid() = default;
  explicit pid(pid_t val) : mPid(val) {}

  void wait() {
    int status;
    waitpid(mPid, &status, 0);
    // todo add error handling
  }

  ~pid() {
    if (!mWaitedOn) {
      std::cerr << "wait() was not called on fork " << mPid << "\n";
      exit(-1);
    }
  }

private:
  pid_t mPid;
  bool mWaitedOn;
};

inline pid fork(const std::function<void()> &func) {
  pid_t pidValue = fork();
  if (pidValue == 0) {
    func();
    return {};
  } else {
    return pid{pidValue};
  }
}
