#include "constants.hpp"
#include "graph.pb.h"

#include "xpti_trace_framework.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <google/protobuf/arena.h>
#include <iostream>
#include <mutex>
#include <set>
#include <string_view>

struct GraphGlobalData {
  google::protobuf::Arena arena;
  std::mutex mutex;
  std::vector<dpcpp_trace::GraphEvent *> graph;
  std::set<int64_t> ids;
  const std::chrono::time_point<std::chrono::steady_clock> start =
      std::chrono::steady_clock::now();
};

GraphGlobalData *data = nullptr;

XPTI_CALLBACK_API void tpCallback(uint16_t trace_type,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *userData);

XPTI_CALLBACK_API void xptiTraceInit(unsigned int major_version,
                                     unsigned int minor_version,
                                     const char *version_str,
                                     const char *streamName) {
  std::string_view nameView{streamName};
  if (nameView == "sycl") {
    char *tstr;
    // Register this stream to get the stream ID; This stream may already have
    // been registered by the framework and will return the previously
    // registered stream ID
    uint8_t GStreamID = xptiRegisterStream(streamName);
    xpti::string_id_t dev_id = xptiRegisterString("sycl_device", &tstr);

    // Register our lone callback to all pre-defined trace point types
    xptiRegisterCallback(
        GStreamID, static_cast<uint16_t>(xpti::trace_point_type_t::node_create), tpCallback);
    xptiRegisterCallback(
        GStreamID, static_cast<uint16_t>(xpti::trace_point_type_t::edge_create), tpCallback);
    xptiRegisterCallback(
        GStreamID, static_cast<uint16_t>(xpti::trace_point_type_t::task_begin),
        tpCallback);
    xptiRegisterCallback(
        GStreamID, static_cast<uint16_t>(xpti::trace_point_type_t::task_end),
        tpCallback);
    data = new GraphGlobalData();
  }
}

static void dumpGraph() {
  std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};

  std::ofstream traceFile{outDir / "sycl.graph_trace"};

  for (auto *event : data->graph) {
    std::string out;
    event->SerializeToString(&out);

    uint32_t size = out.size();
    traceFile.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
    traceFile.write(out.data(), size);
  }

  traceFile.close();
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *StreamName) {
  if (std::string_view{StreamName} == "sycl") {
    dumpGraph();
    delete data;
  }
}

std::string truncate(std::string Name) {
  size_t Pos = Name.find_last_of(":");
  if (Pos != std::string::npos) {
    return Name.substr(Pos + 1);
  } else {
    return Name;
  }
}

XPTI_CALLBACK_API void tpCallback(uint16_t traceType,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *userData) {
  const auto end = std::chrono::steady_clock::now();

  std::lock_guard lock{data->mutex};
  auto payload = xptiQueryPayload(event);

  std::string name;

  if (payload->name_sid() != xpti::invalid_id) {
    name = truncate(payload->name);
  } else {
    name = "<unknown>";
  }

  int64_t id = event ? event->unique_id : 0;

  const auto findById = [](int64_t id) {
    return std::find_if(
        data->graph.rbegin(), data->graph.rend(),
        [id](const dpcpp_trace::GraphEvent *evt) { return evt->id() == id; });
  };

  const auto timestamp = [](auto end) -> uint64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                 data->start)
        .count();
  };

  if (traceType ==
      static_cast<uint16_t>(xpti::trace_point_type_t::task_begin)) {
    auto it = findById(id);
    if (it != data->graph.rend()) {
      (*it)->set_time_start(timestamp(end));
    }
    return;
  }

  if (traceType == static_cast<uint16_t>(xpti::trace_point_type_t::task_end)) {
    auto it = findById(id);
    if (it != data->graph.rend()) {
      (*it)->set_time_end(timestamp(end));
    }
  }

  if (data->ids.count(id) > 0) {
    return;
  }
  data->ids.insert(id);

  auto *graphEvent =
      google::protobuf::Arena::CreateMessage<dpcpp_trace::GraphEvent>(
          &data->arena);
  graphEvent->set_id(id);
  graphEvent->set_time_create(timestamp(end));

  if (traceType ==
      static_cast<uint16_t>(xpti::trace_point_type_t::node_create)) {
    graphEvent->set_type(dpcpp_trace::GraphEvent_EventType_NODE);
  }
  if (traceType ==
      static_cast<uint16_t>(xpti::trace_point_type_t::edge_create)) {
    graphEvent->set_type(dpcpp_trace::GraphEvent_EventType_EDGE);
    graphEvent->set_start_node(event->source_id);
    graphEvent->set_end_node(event->target_id);
  }

  xpti::metadata_t *metadata = xptiQueryMetadata(event);
  for (auto &item : *metadata) {
    auto *protoMetadata =
        google::protobuf::Arena::CreateMessage<dpcpp_trace::Metadata>(
            &data->arena);
    protoMetadata->set_name(xptiLookupString(item.first));
    protoMetadata->set_value(xptiLookupString(item.second));
    graphEvent->mutable_metadata()->AddAllocated(protoMetadata);
  }

  data->graph.push_back(graphEvent);
}
