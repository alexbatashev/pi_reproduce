#pragma once

#include <boost/asio/awaitable.hpp>
#include <cstdint>

class DebugServer {
public:
  DebugServer(uint16_t port = 1111);
  boost::asio::awaitable<void> run();

private:
  uint16_t mPort;
};
