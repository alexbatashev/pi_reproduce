#include "Recorder.hpp"
#include "constants.hpp"
#include "utils/tls.hpp"

#include <xpti_trace_framework.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static dpcpp_trace::TLSKey piKey;
static dpcpp_trace::TLSKey oclKey;

inline constexpr uint16_t funcWithArgsBegin =
    static_cast<uint16_t>(xpti::trace_point_type_t::function_with_args_begin);
inline constexpr uint16_t funcWithArgsEnd =
    static_cast<uint16_t>(xpti::trace_point_type_t::function_with_args_end);

XPTI_CALLBACK_API void piCallback(uint16_t traceType,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *userData);
XPTI_CALLBACK_API void oclCallback(uint16_t traceType,
                                   xpti::trace_event_data_t *parent,
                                   xpti::trace_event_data_t *event,
                                   uint64_t instance, const void *userData);

XPTI_CALLBACK_API void xptiTraceInit(unsigned int /* majorVersion */,
                                     unsigned int /* minorVersion */,
                                     const char * /*versionStr*/,
                                     const char *streamName) {
  if (!std::getenv(kTracePathEnvVar)) {
    std::cerr << kTracePathEnvVar << " is not set\n";
    std::terminate();
  }

  if (std::string_view(streamName) == kPIDebugStreamName) {
    uint8_t streamID = xptiRegisterStream(streamName);
    xptiRegisterCallback(streamID, funcWithArgsBegin, piCallback);
    xptiRegisterCallback(streamID, funcWithArgsEnd, piCallback);
  } else if (std::string_view(streamName) == kOCLDebugStreamName) {
    uint8_t streamID = xptiRegisterStream(streamName);
    xptiRegisterCallback(streamID, funcWithArgsBegin, oclCallback);
    xptiRegisterCallback(streamID, funcWithArgsEnd, oclCallback);
  }
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *streamName) {
  if (std::string_view(streamName) == kPIDebugStreamName) {
    auto *recorder = tls_get<dpcpp_trace::Recorder>(piKey);
    if (recorder) {
      recorder->flush();
      delete recorder;
    }
  }
}

void commonRecord(dpcpp_trace::Recorder *recorder, uint16_t traceType,
                  uint64_t timestamp, xpti::trace_event_data_t *event,
                  uint64_t instance, const void *userData) {
  const auto *data = static_cast<const xpti::function_with_args_t *>(userData);
  if (traceType == funcWithArgsBegin) {
    recorder->recordBegin(0 /*universal id*/, instance, data->function_id,
                          data->function_name, timestamp, data->args_data);
  } else if (traceType == funcWithArgsEnd) {
    recorder->recordEnd(timestamp, data->ret_data);
  } }
XPTI_CALLBACK_API void piCallback(uint16_t traceType,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *userData) {
  using namespace dpcpp_trace;
  tls_initialize<Recorder>(piKey, [](Recorder *rec) {
    if (rec) {
      rec->flush();
      delete rec;
    }
  });

  if (auto *data = tls_get<Recorder>(piKey); data == nullptr) {
    // TODO generic thread name
    std::array<char, 128> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};
    filename += kPiTraceExt;
    auto path = fs::path{std::getenv(kTracePathEnvVar)} / filename;
    auto *recorder = new Recorder(path, APICall::PI);
    tls_set(piKey, recorder);
  }

  auto *recorder = tls_get<Recorder>(piKey);

  commonRecord(recorder, traceType, 0, event, instance, userData);
}

XPTI_CALLBACK_API void oclCallback(uint16_t traceType,
                                   xpti::trace_event_data_t *parent,
                                   xpti::trace_event_data_t *event,
                                   uint64_t instance, const void *userData) {
  using namespace dpcpp_trace;
  tls_initialize<Recorder>(oclKey, [](Recorder *rec) {
    if (rec) {
      rec->flush();
      delete rec;
    }
  });

  if (auto *data = tls_get<Recorder>(oclKey); data == nullptr) {
    // TODO generic thread name
    std::array<char, 128> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};
    filename += kOclTraceExt;
    auto path = fs::path{std::getenv(kTracePathEnvVar)} / filename;
    auto *recorder = new Recorder(path, APICall::OPENCL);
    tls_set(oclKey, recorder);
  }

  auto *recorder = tls_get<Recorder>(oclKey);

  commonRecord(recorder, traceType, 0, event, instance, userData);
}

// SYCL runtime uses value of 110. Priority of 109 ensures, that SYCL runtime
// has shut down.
__attribute__((destructor(109))) static void collectorUnload() {
  // OpenCL standard doesn't provide any APIs to indicate, that user has
  // finished working with the library. API calls can come at any time. Hence,
  // flush recorder in library destructor.
  auto *recorder = tls_get<dpcpp_trace::Recorder>(oclKey);
  if (recorder) {
    recorder->flush();
    delete recorder;
  }
}
