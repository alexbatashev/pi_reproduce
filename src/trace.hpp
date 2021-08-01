#pragma once

#include "fork.hpp"

#include <cstdint>
#include <functional>
#include <string_view>

class OpenHandler {
public:
  OpenHandler(pid p, std::uintptr_t stackAddr, unsigned long fileNameReg)
      : mPid(p), mStackAddr(stackAddr), mFileNameRegister(fileNameReg) {}
  void execute() const;
  void execute(std::string_view newFile) const;

private:
  pid mPid;
  std::uintptr_t mStackAddr;
  unsigned long mFileNameRegister;
};

class Tracer {
public:
  Tracer(pid);

  void onFileOpen(
      std::function<void(const std::string &, const OpenHandler &)> handler) {
    mFileOpenHandler = handler;
  }

  void run();

private:
  std::function<void(const std::string &, const OpenHandler &)>
      mFileOpenHandler =
          [](const std::string &, const OpenHandler &h) { h.execute(); };
  pid mPid;
};

void traceMe();
