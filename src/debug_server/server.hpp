#pragma once

#include "debug/AbstractDebugger.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

class DebugServer {
public:
  DebugServer(uint16_t port = 1111);
  void run();

  void addDebugger(std::shared_ptr<dpcpp_trace::AbstractDebugger> debugger) {
    mDebuggers.push_back(std::move(debugger));
  }

private:
  std::string processMessage(std::string_view message);

  uint16_t mPort;
  std::vector<std::shared_ptr<dpcpp_trace::AbstractDebugger>> mDebuggers;
};
