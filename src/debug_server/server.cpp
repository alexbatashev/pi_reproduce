#include "server.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <fmt/core.h>
#include <iostream>
#include <iterator>
#include <span>

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

namespace dpcpp_trace {
void DebugServer::run() {
  boost::asio::io_context ctx{1};
  boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){ ctx.stop(); });

  const auto runner = [this]() -> awaitable<void> {
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), mPort});
    tcp::socket socket = co_await acceptor.async_accept(use_awaitable);

    std::array<char, 16384> buf;

    while (true) {
      std::size_t n = co_await socket.async_read_some(boost::asio::buffer(buf),
                                                      use_awaitable);
      std::string_view message(buf.data(), n);
      std::string response = mProtocol->processPacket(message);
      if (!response.empty())
        co_await async_write(socket, boost::asio::buffer(response),
                             use_awaitable);
      fmt::print("-> {}\n", message);
      fmt::print("<- {}\n", response);
      if (!mProtocol->isRunning())
        break;
    }

    co_return;
  };

  co_spawn(ctx, runner, detached);

  ctx.run();
}
} // namespace dpcpp_trace
