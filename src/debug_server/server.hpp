#pragma once

#include "debug/AbstractDebugger.hpp"
#include "protocol.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace dpcpp_trace {
class DebugServer {
public:
  DebugServer() = default;
  template <typename ProtocolTagTy>
  DebugServer(ProtocolTagTy, uint16_t port = 1111) : mPort(port) {
    mProtocol =
        std::make_unique<typename protocol_traits<ProtocolTagTy>::type>(*this);
  }

  void run();

  void addDebugger(std::shared_ptr<dpcpp_trace::AbstractDebugger> debugger) {
    mDebuggers.push_back(std::move(debugger));
  }

  std::shared_ptr<dpcpp_trace::AbstractDebugger> &getActiveDebugger() {
    return mDebuggers[mActiveDebugger];
  }

private:
  uint16_t mPort;
  size_t mActiveDebugger = 0;
  std::vector<std::shared_ptr<dpcpp_trace::AbstractDebugger>> mDebuggers;
  std::unique_ptr<Protocol> mProtocol;
};
} // namespace dpcpp_trace
