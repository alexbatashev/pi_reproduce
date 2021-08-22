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
  std::string process_QStartNoAckMode(std::string_view packet);
  std::string process_g(std::string_view packet);
  std::string process_H(std::string_view packet);
  std::string process_qfThreadInfo(std::string_view packet);
  std::string process_qC(std::string_view packet);
  std::string process_m(std::string_view packet);
  std::string process_qXfer(std::string_view packet);
  std::string process_z(std::string_view packet);
  std::string process_Z(std::string_view packet);
  std::string process_vCont(std::string_view packet);
  std::string process_vFile(std::string_view packet);
  std::string processStatus(std::string_view packet);
  std::string processUnsupported(std::string_view packet);
  std::string processAck(std::string_view packet);

  bool mAck = true;

  size_t mCurrentThreadIndex = 0;
  size_t mLastThreadInfoIndex = 0;

  DebugServer &mServer;
};

template <> struct protocol_traits<gdb_server_tag> {
  using type = GDBServerProtocol;
};
} // namespace dpcpp_trace
