#pragma once

#include <string>
#include <string_view>

namespace dpcpp_trace {
template <typename T> struct protocol_traits;

class Protocol {
public:
  Protocol() = default;

  virtual std::string processPacket(std::string_view packet) = 0;

  virtual bool isRunning() const = 0;

  virtual ~Protocol() = default;
};
} // namespace dpcpp_trace
