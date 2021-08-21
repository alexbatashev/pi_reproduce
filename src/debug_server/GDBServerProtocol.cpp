#include "GDBServerProtocol.hpp"
#include "server.hpp"

#include <ctre.hpp>
#include <fmt/core.h>
#include <lldb/Utility/GDBRemote.h>
#include <lldb/Utility/StringExtractorGDBRemote.h>

using namespace lldb_private;

namespace dpcpp_trace {
GDBServerProtocol::GDBServerProtocol(DebugServer &server) : mServer(server) {}

constexpr unsigned char checksum(std::string_view message) {
  unsigned char checksum = 0;
  for (auto c : message)
    checksum += c;
  return checksum;
}

static std::string createResponse(std::string_view response, bool ack = true) {
  std::string result = "";

  if (ack)
    result += "+";

  fmt::format_to(std::back_inserter(result), "${}#{:02x}", response,
                 checksum(response));

  return result;
}

std::string GDBServerProtocol::processPacket(std::string_view packet) {
  llvm::StringRef data(packet.data(), packet.size());

  if (ctre::match<"\\+?\\$qAttached(?::[0-9a-zA-Z]+)?#[0-9a-fA-F]{2}\\+?">(
          packet)) {
    return process_qAttached(packet);
  } else if (ctre::match<
                 "\\+?\\$qSupported:[0-9a-zA-Z\\+\\-;=]+#[0-9a-fA-F]{2}\\+?">(
                 packet)) {
    return process_qSupported(packet);
  } else if (ctre::match<"\\+?\\$QStartNoAckMode#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_QStartNoAckMode(packet);
  } else if (ctre::match<"\\+?\\$\\?#[0-9a-fA-F]{2}\\+?">(packet)) {
    return processStatus(packet);
  } else if (ctre::match<"\\+?\\$g#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_g(packet);
  } else if (ctre::match<"\\+?\\$H.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_H(packet);
  } else if (packet == "+") {
    return processAck(packet);
  } else {
    return processUnsupported(packet);
  }
}

std::string GDBServerProtocol::process_qSupported(std::string_view) {
  // TODO enable noack mode
  const std::string supportedFeatures =
      "swbreak+;hwbreak+;PacketSize=8191;QStartNoAckMode+";
  return createResponse(supportedFeatures, mAck);
}

std::string GDBServerProtocol::processAck(std::string_view) {
  if (mAck)
    return "+";

  return "";
}

std::string
GDBServerProtocol::process_QStartNoAckMode(std::string_view packet) {
  mAck = false;
  return createResponse("OK", true);
}

std::string GDBServerProtocol::process_H(std::string_view packet) {
  auto [match, threadIdHex] =
      ctre::match<"\\+?\\$Hg([0-9a-fA-F]+)#[0-9a-fA-F]{2}">(packet);
  if (match) {
    std::string idStr = "0x" + threadIdHex.str();
    mCurrentThreadIndex = std::stoul(idStr, nullptr, 16);

    return createResponse("OK", mAck);
  }
  return createResponse("E01", mAck);
}

std::string GDBServerProtocol::process_g(std::string_view packet) {
  std::vector<uint8_t> regValues =
      mServer.getActiveDebugger()->getRegistersData(mCurrentThreadIndex);
  std::string response;
  response.reserve(regValues.size() * 2);
  for (auto v : regValues) {
    constexpr std::array<char, 16> hexMap = {'0', '1', '2', '3', '4', '5',
                                             '6', '7', '8', '9', 'a', 'b',
                                             'c', 'd', 'e', 'f'};
    response += hexMap[(v >> 4) & 0xf];
    response += hexMap[(v >> 0) & 0xf];
  }
  return createResponse(response, mAck);
}

std::string GDBServerProtocol::processStatus(std::string_view packet) {
  // TODO reply real status
  return createResponse("S05", mAck);
}

std::string GDBServerProtocol::process_qAttached(std::string_view) {
  // TODO query debugger for process status.
  const std::string attached =
      mServer.getActiveDebugger()->isAttached() ? "1" : "0";
  return createResponse(attached, mAck);
}

std::string GDBServerProtocol::processUnsupported(std::string_view) {
  return createResponse("", mAck);
}

} // namespace dpcpp_trace
