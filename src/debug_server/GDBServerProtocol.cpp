#include "GDBServerProtocol.hpp"
#include "server.hpp"

#include <ctre.hpp>
#include <lldb/Utility/GDBRemote.h>
#include <lldb/Utility/StringExtractorGDBRemote.h>

using namespace lldb_private;

namespace dpcpp_trace {
GDBServerProtocol::GDBServerProtocol(DebugServer &server) : mServer(server) {}

std::string GDBServerProtocol::processPacket(std::string_view packet) {
  llvm::StringRef data(packet.data(), packet.size());
  if (ctre::match<"qAttached">(packet))
    return process_qAttached(packet);
  else if (ctre::match<"qSupported">(packet))
    return process_qSupported(packet);
  else
    return processUnsupported(packet);
}

std::string GDBServerProtocol::process_qSupported(std::string_view) {
  const std::string supportedFeatures =
      "multiprocess+;swbreak+;hwbreak+;PacketSize=8192;query-attached+";
  StreamGDBRemote response;
  response.PutEscapedBytes(supportedFeatures.data(), supportedFeatures.size());
  return response.GetString().str();
}

std::string GDBServerProtocol::process_qAttached(std::string_view) {
  // TODO query debugger for process status.
  const std::string attached =
      mServer.getActiveDebugger()->isAttached() ? "1" : "0";
  StreamGDBRemote response;
  response.PutEscapedBytes(attached.data(), attached.size());
  return response.GetString().str();
}

std::string GDBServerProtocol::processUnsupported(std::string_view) {
  return "+$#00";
}

} // namespace dpcpp_trace
