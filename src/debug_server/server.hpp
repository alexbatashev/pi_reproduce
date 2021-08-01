#pragma once

#include <cstdint>
#include <string_view>

class DebugServer {
public:
  DebugServer(uint16_t port = 1111);
  void run();

private:
  std::string processMessage(std::string_view message);
  uint16_t mPort;
};
