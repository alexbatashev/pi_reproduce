#pragma once

#include "protocol.hpp"

#include <string>
#include <string_view>

class StringExtractorGDBRemote;

namespace dpcpp_trace {
struct gdb_server_tag {};

class DebugServer;
class GDBServerProtocol : public Protocol {
public:
  GDBServerProtocol(DebugServer &server);

  std::string processPacket(std::string_view packet) final;

private:
  std::string process_qSupported(std::string_view packet);
  std::string process_qAttached(std::string_view packet);
  std::string processUnsupported(std::string_view packet);

  DebugServer &mServer;
};

template <> struct protocol_traits<gdb_server_tag> {
  using type = GDBServerProtocol;
};
} // namespace dpcpp_trace
