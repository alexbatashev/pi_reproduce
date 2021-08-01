#include "server.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <array>
#include <iostream>
#include <fmt/core.h>
#include <iterator>

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;
#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

DebugServer::DebugServer(uint16_t port) : mPort(port) {}

constexpr unsigned char checksum(std::string_view message) {
  unsigned char checksum = 0;
  for (auto c : message) checksum += c;
  return checksum;
}

std::string DebugServer::processMessage(std::string_view message) {
  if (message.find("qSupported") != std::string::npos) {
    std::string supportedFeatures = "multiprocess+;swbreak+;hwbreak+;PacketSize=8192;query-attached+";
    unsigned char chsm = checksum(supportedFeatures);
    std::string response = "";
    fmt::format_to(std::back_inserter(response), "+${}#{:x}", supportedFeatures, chsm);
    return response;
  } else if (message.find("$?#3f") != std::string::npos) {
    return "+$S05#b8";
  } else if (message.find("qAttached") != std::string::npos) {
    std::string reply = "1";
    std::string response = "";
    fmt::format_to(std::back_inserter(reply), "+${}${:x}", reply, checksum(reply));
    return response;
  } else if (message == "+") {
    return "";
  }

  return "+$#00";
}

void DebugServer::run() {
  boost::asio::io_context ctx{1};
  boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){ ctx.stop(); });

  const auto runner = [this]() -> awaitable<void> {
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), mPort});
    tcp::socket socket = co_await acceptor.async_accept(use_awaitable);

    std::array<char, 8192> buf;

    while (true) {
      std::size_t n = co_await socket.async_read_some(boost::asio::buffer(buf), use_awaitable);
      std::cout << "Got some data: \n";
      std::string_view message(buf.data(), n);
      std::cout << message << std::endl;
      std::string response = processMessage(message);
      std::cout << "Response: " << response << "\n";
      if (!response.empty())
        co_await async_write(socket, boost::asio::buffer(response), use_awaitable);
    }

    co_return;
  };

  co_spawn(ctx, runner, detached);

  ctx.run();
}
