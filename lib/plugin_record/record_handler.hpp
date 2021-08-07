#pragma once

#include "pi_arguments_handler.hpp"
#include "xpti_trace_framework.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <ostream>

class RecordHandler {
public:
  RecordHandler(std::unique_ptr<std::ostream> os,
                std::chrono::time_point<std::chrono::steady_clock> timestamp,
                bool skipMemObjects);

  void handle(uint64_t eventId, uint32_t funcId, const pi_plugin &plugin,
              std::optional<pi_result> result, void *data);
  void flush();

  void timestamp_begin();
  void timestamp_end();

private:
  sycl::xpti_helpers::PiArgumentsHandler mArgHandler;
  std::unique_ptr<std::ostream> mOS;
  std::chrono::time_point<std::chrono::steady_clock> mStartTime;
  uint64_t mLastEventId;
  uint32_t mLastFunctionId;
  uint64_t mTimestampBegin;
  uint64_t mTimestampEnd;
  bool mSkipMemObjects;
};
