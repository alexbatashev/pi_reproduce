#include "server.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

DebugServer::DebugServer(uint16_t port) : mPort(port) {}

boost::asio::awaitable<void> DebugServer::run() {
  boost::asio::io_context ctx;

  co_return;
}
