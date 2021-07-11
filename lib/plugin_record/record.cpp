#include "constants.hpp"
#include "record_handler.hpp"
#include "write_utils.hpp"

#include <CL/sycl/detail/xpti_plugin_info.hpp>
#include "pi_arguments_handler.hpp"
#include "xpti_trace_framework.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <pthread.h>
#include <string>

static uint8_t GStreamID = 0;

// TODO free memory
thread_local RecordHandler *GRecordHandler;

std::chrono::time_point<std::chrono::steady_clock> GStartTime;

static bool shouldSkipMemObjects() {
  static bool res = getenv(kSkipMemObjsEnvVar) != nullptr;
  return res;
}

XPTI_CALLBACK_API void tpCallback(uint16_t trace_type,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *user_data);

XPTI_CALLBACK_API void xptiTraceInit(unsigned int major_version,
                                     unsigned int minor_version,
                                     const char *version_str,
                                     const char *stream_name) {
  if (!std::getenv("PI_REPRODUCE_TRACE_PATH")) {
    std::cerr << "PI_REPRODUCE_TRACE_PATH is not set\n";
    std::terminate();
  }

  if (std::string_view(stream_name) == "sycl.pi.arg") {
    GStreamID = xptiRegisterStream(stream_name);
    xptiRegisterCallback(
        GStreamID, (uint16_t)xpti::trace_point_type_t::function_with_args_begin,
        tpCallback);
    xptiRegisterCallback(
        GStreamID, (uint16_t)xpti::trace_point_type_t::function_with_args_end,
        tpCallback);

    GStartTime = std::chrono::steady_clock::now();
  }
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *stream_name) {
  // NOP
}

XPTI_CALLBACK_API void tpCallback(uint16_t TraceType,
                                  xpti::trace_event_data_t *Parent,
                                  xpti::trace_event_data_t *Event,
                                  uint64_t Instance, const void *UserData) {
  auto Type = static_cast<xpti::trace_point_type_t>(TraceType);
  if (Type == xpti::trace_point_type_t::function_with_args_begin) {
    const auto start = std::chrono::steady_clock::now();
    if (GRecordHandler == nullptr) {
      std::filesystem::path outDir{std::getenv("PI_REPRODUCE_TRACE_PATH")};
      std::array<char, 1024> buf;
      pthread_getname_np(pthread_self(), buf.data(), buf.size());
      std::string filename{buf.data()};
      filename += ".trace";
      auto fs = std::make_unique<std::ofstream>(
          outDir / filename, std::ios::out | std::ios::app | std::ios::binary);
      GRecordHandler =
          new RecordHandler(std::move(fs), GStartTime, shouldSkipMemObjects());
    }

    if (GRecordHandler) {
      const auto *Data =
          static_cast<const xpti::function_with_args_t *>(UserData);
      const auto *Plugin =
          static_cast<sycl::detail::XPTIPluginInfo *>(Data->user_data);

      GRecordHandler->writeHeader(Data->function_id, Plugin->backend);
      GRecordHandler->timestamp();
    }
  } else if (Type == xpti::trace_point_type_t::function_with_args_end &&
             GRecordHandler) {
    GRecordHandler->timestamp();

    const auto *Data =
        static_cast<const xpti::function_with_args_t *>(UserData);

    const auto *Plugin =
        static_cast<sycl::detail::XPTIPluginInfo *>(Data->user_data);
    const pi_result Result = *static_cast<pi_result *>(Data->ret_data);
    GRecordHandler->handle(Data->function_id, *Plugin, Result, Data->args_data);

    GRecordHandler->flush();
  }
}
