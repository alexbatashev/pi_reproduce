#include "GDBServerProtocol.hpp"
#include "server.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ctre.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <sys/stat.h>
#include <type_traits>
#include <vector>

#include <fcntl.h>

namespace fs = std::filesystem;

namespace dpcpp_trace {
template <typename T>
concept ByteSequence = std::ranges::forward_range<T> &&
    std::is_same_v<uint8_t, std::ranges::range_value_t<T>>;

GDBServerProtocol::GDBServerProtocol(DebugServer &server) : mServer(server) {}

static std::string encode(ByteSequence auto data) {
  std::string response;
  response.reserve(data.size() * 2);
  for (auto v : data) {
    constexpr std::array<char, 16> hexMap = {'0', '1', '2', '3', '4', '5',
                                             '6', '7', '8', '9', 'a', 'b',
                                             'c', 'd', 'e', 'f'};
    response += hexMap[(v >> 4) & 0xf];
    response += hexMap[(v >> 0) & 0xf];
  }
  return response;
}

static std::string encode(std::string_view data) {
  return encode(std::span{reinterpret_cast<const uint8_t*>(data.data()), data.size()});
}

constexpr int hexDigitToInt(char c) {
  if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';
  if (c >= 'A' && c <= 'F')
    return 10 + c - 'a';
  if (c >= '0' && c <= '9')
    return c - '0';
  return -1;
}

static std::vector<uint8_t> decode(std::string_view data) {
  std::vector<uint8_t> buffer;
  buffer.reserve(data.size() / 2);

  for (size_t idx = 0; idx < data.size(); idx += 2) {
    uint8_t num = 0;
    num += hexDigitToInt(data[idx]) << 4;
    num += hexDigitToInt(data[idx + 1]);
    buffer.push_back(num);
  }
  return buffer;
}

static std::string escape(ByteSequence auto data) {
  std::string response;
  response.reserve(std::ranges::size(data));

  for (auto c : data) {
    if (c == 0x23 || c == 0x24 || c == 0x7d || c == 0x2a) {
      response.push_back('}');
      response.push_back(c xor 0x20);
    } else {
      response.push_back(c);
    }
  }

  return response;
}

std::string trim(std::string_view v) {
  auto it = std::ranges::find_if(v, [](char c) { return c != '0'; });

  if (it == v.end())
    return "0";

  return {it, v.end()};
}

constexpr std::string encode(std::integral auto num) {
  std::span<uint8_t> bytes(reinterpret_cast<uint8_t *>(&num), sizeof(num));
  return trim(encode(bytes | std::views::reverse));
}

template <std::integral I> constexpr I parseInt(std::string_view num) {
  if (num.size() > sizeof(I) * 2)
    return std::numeric_limits<I>::max();

  I result = 0;
  for (auto c : num) {
    uint8_t nibble = hexDigitToInt(c);
    result <<= 4;
    result |= nibble;
  }

  return result;
}

std::string decodeString(std::string_view str) {
  std::string ret;
  ret.reserve(str.size() / 2);

  bool flip = false;
  char realChar = 0;

  for (auto c : str) {
    if (flip) {
      realChar += hexDigitToInt(c);
      ret.push_back(realChar);
      flip = false;
    } else {
      realChar = hexDigitToInt(c);
      realChar <<= 4;
      flip = true;
    }
  }

  return ret;
}

int llopen(fs::path path, int flags, int mode) {
  return open(path.c_str(), flags, mode);
}

int llread(int fd, void *buf, size_t offset, size_t count) {
  lseek(fd, offset, SEEK_SET);
  return read(fd, buf, count);
}

int llclose(int fd) { return close(fd); }

std::string llstat(int fd) {
  struct stat buf;
  int res = fstat(fd, &buf);

  if (res == -1)
    return "";

  return escape(
      std::span<uint8_t>{reinterpret_cast<uint8_t *>(&buf), sizeof(buf)});
}

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
  if (ctre::match<"\\+?\\$qAttached(?::[0-9a-zA-Z]+)?#[0-9a-fA-F]{2}\\+?">(
          packet)) {
    return process_qAttached(packet);
  } else if (ctre::match<"\\+?\\$qSupported:.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_qSupported(packet);
  } else if (ctre::match<"\\+?\\$QStartNoAckMode#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_QStartNoAckMode(packet);
  } else if (ctre::match<"\\+?\\$\\?#[0-9a-fA-F]{2}\\+?">(packet)) {
    return processStatus(packet);
  } else if (ctre::match<"\\+?\\$g#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_g(packet);
  } else if (ctre::match<"\\+?\\$G[0-9a-f]+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_G(packet);
  } else if (ctre::match<"\\+?\\$H.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_H(packet);
  } else if (ctre::match<"\\+?\\$qfThreadInfo#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_qfThreadInfo(packet);
  } else if (ctre::match<"\\+?\\$qsThreadInfo#[0-9a-fA-F]{2}\\+?">(packet)) {
    // This is intentional, handling for qf and qs is the same.
    return process_qfThreadInfo(packet);
  } else if (ctre::match<"\\+?\\$qC#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_qC(packet);
  } else if (ctre::match<"\\+?\\$m[0-9a-fA-F,]+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_m(packet);
  } else if (ctre::match<"\\+?\\$M.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_M(packet);
  } else if (ctre::match<"\\+?\\$x[0-9a-fA-F,]+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_m(packet);
  } else if (ctre::match<"\\+?\\$qXfer.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_qXfer(packet);
  } else if (ctre::match<"\\+?\\$z.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_z(packet);
  } else if (ctre::match<"\\+?\\$Z.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_Z(packet);
  } else if (ctre::match<"\\+?\\$vCont.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_vCont(packet);
  } else if (ctre::match<"\\+?\\$vFile.+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_vFile(packet);
  } else if (ctre::match<"\\+?\\$c#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_c(packet);
  } else if (ctre::match<"\\+?\\$p[0-9a-f]+#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_p(packet);
  } else if (ctre::match<"\\+?\\$qProcessInfo#[0-9a-fA-F]{2}\\+?">(packet)) {
    return process_qProcessInfo(packet);
  } else if (packet == "+") {
    return processAck(packet);
  } else {
    return processUnsupported(packet);
  }
}

std::string GDBServerProtocol::process_qSupported(std::string_view) {
  // TODO enable noack mode
  const std::string supportedFeatures = fmt::format(
      "swbreak+;hwbreak+;vContSupported+;PacketSize={:x};QStartNoAckMode+;"
      "qXfer:auxv:read+;qXfer:features:read+;qXfer:exec-file:read+",
      16384);
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
      ctre::match<"\\+?\\$H[gc]([\\-0-9a-fA-F]+)#[0-9a-fA-F]{2}">(packet);
  if (match) {
    if (threadIdHex.str()[0] == '-')
      mCurrentThreadIndex = 0;
    else
      mCurrentThreadIndex = parseInt<uint64_t>(threadIdHex);

    return createResponse("OK", mAck);
  }
  return createResponse("E01", mAck);
}

std::string GDBServerProtocol::process_qfThreadInfo(std::string_view packet) {
  auto &debugger = mServer.getActiveDebugger();
  if (mLastThreadInfoIndex < debugger->getNumThreads()) {
    // TODO encode hex properly
    std::string response = fmt::format(
        "m{}", encode(debugger->getThreadIDAtIndex(mLastThreadInfoIndex++)));
    return createResponse(response, mAck);
  } else {
    mLastThreadInfoIndex = 0;
    return createResponse("l", mAck);
  }
}

std::string GDBServerProtocol::process_qC(std::string_view packet) {
  auto &debugger = mServer.getActiveDebugger();
  std::string response;
  fmt::format_to(std::back_inserter(response), "QC{}",
                 encode(debugger->getThreadIDAtIndex(mCurrentThreadIndex)));
  return createResponse(response, mAck);
}

std::string GDBServerProtocol::process_g(std::string_view packet) {
  std::vector<uint8_t> regValues =
      mServer.getActiveDebugger()->getRegistersData(mCurrentThreadIndex);
  if (regValues.empty())
    return createResponse("E01", mAck);
  return createResponse(encode(regValues), mAck);
}

std::string GDBServerProtocol::process_G(std::string_view packet) {
  auto regValues = decode(packet.substr(1));
  mServer.getActiveDebugger()->writeRegistersData(regValues,
                                                  mCurrentThreadIndex);
  return createResponse("OK", mAck);
}

std::string GDBServerProtocol::process_m(std::string_view packet) {
  auto [match, startHex, lengthHex] =
      ctre::match<"\\+?\\$[mx]([0-9a-f]+),([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
          packet);

  uint64_t start = parseInt<uint64_t>(startHex);
  uint64_t length = parseInt<uint64_t>(lengthHex);

  // LLDB is testing us, don't be fooled.
  if (length == 0)
    return createResponse("OK", mAck);

  std::vector<uint8_t> data =
      mServer.getActiveDebugger()->readMemory(start, length);
  if (data.empty())
    return createResponse("E01", mAck);
  if (packet.starts_with("$x"))
    return createResponse(escape(data), mAck);
  return createResponse(encode(data), mAck);
}

std::string GDBServerProtocol::process_M(std::string_view packet) {
  auto [match, startHex, lengthHex, dataHex] = ctre::match<
      "\\+?\\$[mx]([0-9a-f]+),([0-9a-f]+):([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
      packet);

  uint64_t start = parseInt<uint64_t>(startHex);
  uint64_t length = parseInt<uint64_t>(lengthHex);

  // LLDB is testing us, don't be fooled.
  if (length == 0)
    return createResponse("OK", mAck);

  auto data = decode(dataHex);

  mServer.getActiveDebugger()->writeMemory(start, length, data);
  return createResponse("OK", mAck);
}

static std::string encodeStopReason(StopReason stopReason) {
  if (stopReason.type == StopReason::Type::signal) {
    std::cout << "SIGNAL\n";
    std::string response = fmt::format("S{:02x}", stopReason.code);
    return response;
  } else if (stopReason.type == StopReason::Type::breakpoint) {
    std::string response = "T05swbreak:;";
    return response;
  } else if (stopReason.type == StopReason::Type::exit) {
    return "W00";
  } else if (stopReason.type == StopReason::Type::watchpoint) {
    std::cout << "WATCHPOINT\n";
  } else if (stopReason.type == StopReason::Type::exception) {
    std::cout << "EXCEPTION\n";
  } else if (stopReason.type == StopReason::Type::fork) {
    std::cout << "FORK\n";
  } else if (stopReason.type == StopReason::Type::none) {
    std::cout << "NONE\n";
    return "W00";
  }
  return "S05";
}

std::string GDBServerProtocol::processStatus(std::string_view packet) {
  auto stopReason =
      mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);

  return createResponse(encodeStopReason(stopReason), mAck);
}

std::string GDBServerProtocol::process_qXfer(std::string_view packet) {
  {
    auto [match, offsetHex, lengthHex] =
        ctre::match<"\\+?\\$qXfer:features:read:target\\.xml:([0-9a-f]+),([0-"
                    "9a-f]+)#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      size_t offset = parseInt<uint64_t>(offsetHex);
      size_t length = parseInt<uint64_t>(lengthHex);
      auto xml = mServer.getActiveDebugger()->getGDBTargetXML();

      if (offset >= xml.size()) {
        return createResponse("", mAck);
      }

      std::string slice = xml.substr(offset, length);
      if (offset + length <= xml.size()) {
        slice = "m" + slice;
      } else {
        slice = "l" + slice;
      }

      return createResponse(slice, mAck);
    }
  }
  {
    auto [match, offsetHex, lengthHex] = ctre::match<
        "\\+?\\$qXfer:auxv:read::([0-9a-f]+),([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
        packet);
    if (match) {
      size_t offset = parseInt<uint64_t>(offsetHex);
      size_t length = parseInt<uint64_t>(lengthHex);
      auto auxv = mServer.getActiveDebugger()->getAuxvData();

      if (offset >= auxv.size()) {
        return createResponse("", mAck);
      }

      std::string slice = escape(auxv).substr(offset, length);
      if (offset + length <= auxv.size()) {
        slice = "m" + slice;
      } else {
        slice = "l" + slice;
      }

      return createResponse(slice, mAck);
    }
  }
  {
    auto [match, pidHex, offsetHex, lengthHex] =
        ctre::match<"\\+?\\$qXfer:exec\\-file:read:([0-9a-f]+)?:([0-9a-f]+),(["
                    "0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      size_t offset = parseInt<uint64_t>(offsetHex);
      size_t length = parseInt<uint64_t>(lengthHex);
      auto path = mServer.getActiveDebugger()->getExecutablePath();

      if (offset >= path.size()) {
        return createResponse("", mAck);
      }

      std::string slice = path.substr(offset, length);
      if (offset + length <= path.size()) {
        slice = "m" + slice;
      } else {
        slice = "l" + slice;
      }

      return createResponse(slice, mAck);
    }
  }
  return createResponse("", mAck);
}

std::string GDBServerProtocol::process_z(std::string_view packet) {
  {
    auto [match, addrHex, kindHex] =
        ctre::match<"\\+?\\$z0,([0-9a-f]+),([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
            packet);
    if (match) {
      uint64_t addr = parseInt<uint64_t>(addrHex);

      mServer.getActiveDebugger()->removeSoftwareBreakpoint(addr);

      return createResponse("OK", mAck);
    }
  }
  return createResponse("E01", mAck);
}

std::string GDBServerProtocol::process_Z(std::string_view packet) {
  {
    auto [match, addrHex, kindHex] =
        ctre::match<"\\+?\\$Z0,([0-9a-f]+),([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
            packet);
    if (match) {
      uint64_t addr = parseInt<uint64_t>(addrHex);

      mServer.getActiveDebugger()->createSoftwareBreakpoint(addr);

      return createResponse("OK", mAck);
    }
  }
  return createResponse("E01", mAck);
}

std::string GDBServerProtocol::process_vCont(std::string_view packet) {
  {
    auto match = ctre::match<"\\+?\\$vCont\\?#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      return createResponse("vCont;c;C;t;s;S;r", mAck);
    }
  }
  {
    auto match = ctre::match<"\\+?\\$vCont;c#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      mServer.getActiveDebugger()->resume();
      auto stopReason =
          mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
      return createResponse(encodeStopReason(stopReason), mAck);
    }
  }
  {
    auto [match, tidHex] =
        ctre::match<"\\+?\\$vCont;c:([a-f0-9]+)#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      uint64_t tid = parseInt<uint64_t>(tidHex);
      mServer.getActiveDebugger()->resume(0, tid);
      auto stopReason =
          mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
      return createResponse(encodeStopReason(stopReason), mAck);
    }
  }
  {
    auto [match, signalHex, tidHex] = ctre::match<
        "\\+?\\$vCont;C([0-9a-f]+):([0-9a-f]+)(?:;c)?#[0-9a-fA-F]{2}\\+?">(
        packet);
    if (match) {
      int signal = parseInt<int>(signalHex);
      uint64_t tid = parseInt<uint64_t>(tidHex);
      // TODO what's the meaning of the signal?
      mServer.getActiveDebugger()->resume(signal, tid);
      auto stopReason =
          mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
      return createResponse(encodeStopReason(stopReason), mAck);
    }
  }
  {
    auto [match, tidHex] =
        ctre::match<"\\+?\\$vCont;s:([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      uint64_t tid = parseInt<uint64_t>(tidHex);
      mServer.getActiveDebugger()->stepInstruction(tid);
      auto stopReason =
          mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
      return createResponse(encodeStopReason(stopReason), mAck);
    }
  }
  {
    auto [match, signalHex, tidHex] = ctre::match<
        "\\+?\\$vCont;S([0-9a-f]+):([0-9a-f]+)(?:;c)?#[0-9a-fA-F]{2}\\+?">(
        packet);
    if (match) {
      int signal = parseInt<int>(signalHex);
      uint64_t tid = parseInt<uint64_t>(tidHex);
      // TODO what's the meaning of the signal?
      mServer.getActiveDebugger()->stepInstruction(tid, signal);
      auto stopReason =
          mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
      return createResponse(encodeStopReason(stopReason), mAck);
    }
  }
  return createResponse("", mAck);
}

std::string GDBServerProtocol::process_c(std::string_view) {
  mServer.getActiveDebugger()->resume();
  auto stopReason =
      mServer.getActiveDebugger()->getStopReason(mCurrentThreadIndex);
  return createResponse(encodeStopReason(stopReason), mAck);
}

std::string GDBServerProtocol::process_p(std::string_view packet) {
  auto [match, regHex] = ctre::match<"\\+?\\$p([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(packet);

  size_t regNum = parseInt<size_t>(regHex);

  auto data = mServer.getActiveDebugger()->readRegister(mCurrentThreadIndex, regNum);

  if (data.empty())
    return createResponse("E01", mAck);

  return createResponse(encode(data), mAck);
}

std::string GDBServerProtocol::process_vFile(std::string_view packet) {
  {
    auto match =
        ctre::match<"\\+?\\$vFile:setfs:.+#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      return createResponse("F0", mAck);
    }
  }
  {
    auto [match, nameHex, flagsHex, modeHex] = ctre::match<
        "\\+?\\$vFile:open:(.+),([0-9a-f]+),([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
        packet);
    if (match) {
      int flags = parseInt<int>(flagsHex);
      int mode = parseInt<int>(modeHex);
      fs::path filename = decodeString(nameHex);
      int res = llopen(filename, flags, mode);
      std::string response = "F";
      if (res >= 0) {
        response += encode(res);
      } else {
        response += "-" + encode(-res);
      }
      return createResponse(response, mAck);
    }
  }
  {
    auto [match, fdHex] =
        ctre::match<"\\+?\\$vFile:close:([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(
            packet);
    if (match) {
      std::string_view fdView = fdHex;
      int dropThisMany = fdView[0] == '-' ? 1 : 0;
      int fd = parseInt<int>(fdView.substr(dropThisMany));
      if (dropThisMany > 0)
        fd = -fd;
      int res = llclose(fd);
      std::string response = "F";
      if (res >= 0) {
        response += encode(res);
      } else {
        response += "-" + encode(-res);
      }
      return createResponse(response, mAck);
    }
  }
  /*
  {
    auto [match, fdHex] =
      ctre::match<"\\+?\\$vFile:fstat:([0-9a-f]+)#[0-9a-fA-F]{2}\\+?">(packet);
    if (match) {
      std::string_view fdView = fdHex;
      int fd = parseInt<int>(fdHex);

      std::string data = llstat(fd);
      std::string response = "F";
      if (!data.empty()) {
        response += encode(data.size()) + ";" + data;
      } else {
        response += "-1";
      }
      return createResponse(response, mAck);
    }
  }
  */
  {
    auto [match, fdHex, countHex, offsetHex] =
        ctre::match<"\\+?\\$vFile:pread:([0-9a-f]+),([0-9a-f]+),([0-9a-f]+)#[0-"
                    "9a-fA-F]{2}\\+?">(packet);
    if (match) {
      int fd = parseInt<int>(fdHex);
      size_t count = parseInt<size_t>(countHex);
      size_t offset = parseInt<size_t>(offsetHex);

      std::cout << "Reading " << count << " bytes from F" << fd << "\n";
      std::vector<uint8_t> buf;
      buf.resize(count);

      int res = llread(fd, buf.data(), offset, count);
      buf.resize(res);
      std::string response = "F";
      if (res >= 0) {
        response += encode(res);
        response += ";" + escape(buf);
      } else {
        response += "-" + encode(-res);
      }
      return createResponse(response, mAck);
    }
  }
  return createResponse("", mAck);
}

std::string GDBServerProtocol::process_qAttached(std::string_view) {
  // TODO attaching not supported now.
  const std::string attached =
      mServer.getActiveDebugger()->isAttached() ? "1" : "0";
  return createResponse(attached, mAck);
}

std::string GDBServerProtocol::process_qProcessInfo(std::string_view) {
  // TODO figure out why LLDB fails to resolve breakpoint locations with this
  // extension.
  return createResponse("", mAck);

  ProcessInfo info = mServer.getActiveDebugger()->getProcessInfo();
  std::string response = fmt::format("pid:{:x};prarent-pid:{:x};real-uid:{:x};real-gid:{:x};effective-uid:{:x};effective-gid:{:x};triple:{:s};ostype:{:s};endian:{:s};", info.pid, info.parentPID, info.realUID, info.realGID, info.effectiveUID, info.effectiveGID, encode(info.triple), info.ostype, info.endian);
  if (!info.targetABI.empty()) {
    response += fmt::format("elf_abi:{};", info.targetABI);
  }
  response += fmt::format("ptrsize:{:x}", info.pointerSize);

  return createResponse(response, mAck);
}

std::string GDBServerProtocol::processUnsupported(std::string_view) {
  return createResponse("", mAck);
}

} // namespace dpcpp_trace
