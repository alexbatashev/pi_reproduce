#pragma once

#include "fork.hpp"

#include <functional>
#include <string_view>

class Tracer {
public:
  Tracer(pid);

  void onFileOpen(std::function<void(const std::string &)> handler) {
    mFileOpenHandler = handler;
  }

  void run();

private:
  std::function<void(const std::string &)> mFileOpenHandler;
  pid mPid;
};

void traceMe();
