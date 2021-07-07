#include "xpti_trace_framework.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string_view>

using json = nlohmann::json;

static uint8_t GStreamID = 0;
std::mutex GIOMutex;

json GGraph = json::array();

std::ofstream GDumpFile;

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
    GStreamID = xptiRegisterStream(streamName);
    xpti::string_id_t dev_id = xptiRegisterString("sycl_device", &tstr);

    // Register our lone callback to all pre-defined trace point types
    xptiRegisterCallback(
        GStreamID, (uint16_t)xpti::trace_point_type_t::node_create, tpCallback);
    xptiRegisterCallback(
        GStreamID, (uint16_t)xpti::trace_point_type_t::edge_create, tpCallback);
    xptiRegisterCallback(GStreamID, (uint16_t)xpti::trace_point_type_t::signal,
                         tpCallback);
  }
}

static void dumpGraph() {
  std::filesystem::path outDir{std::getenv("PI_REPRODUCE_TRACE_PATH")};

  std::ofstream traceFile{outDir / "graph.json"};

  traceFile << GGraph.dump(2) << "\n";
  traceFile.close();
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *stream_name) { dumpGraph(); }

__attribute__((destructor)) static void shutdown() {
  // Looks like xptiTraceFinish is never called. Workaround with this.
  dumpGraph();
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
  std::lock_guard lock{GIOMutex};

  auto payload = xptiQueryPayload(event);

  std::string name;

  if (payload->name_sid != xpti::invalid_id) {
    name = truncate(payload->name);
  } else {
    name = "<unknown>";
  }

  uint64_t ID = event ? event->unique_id : 0;

  json obj;

  obj["name"] = name;
  obj["id"] = ID;

  if (traceType ==
      static_cast<uint16_t>(xpti::trace_point_type_t::edge_create)) {
    obj["kind"] = "edge";
  } else {
    obj["kind"] = "node";
  }

  xpti::metadata_t *metadata = xptiQueryMetadata(event);
  for (auto &item : *metadata) {
    obj[xptiLookupString(item.first)] = xptiLookupString(item.second);
  }

  GGraph.push_back(obj);
}
