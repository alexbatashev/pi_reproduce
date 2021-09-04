#include <benchmark/benchmark.h>

#include "graph.pb.h"
#include "utils/MiResource.hpp"

#include <google/protobuf/arena.h>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

static void benchNaive(benchmark::State &state) {
  std::vector<dpcpp_trace::GraphEvent> graph;
  for (auto _ : state) {
    dpcpp_trace::GraphEvent event;
    auto *metadata = event.add_metadata();
    metadata->set_name("device");
    metadata->set_value("SYCL Host device");
    event.set_id(10);
    event.set_type(dpcpp_trace::GraphEvent_EventType_NODE);
    graph.push_back(event);
  }
}

BENCHMARK(benchNaive);

static void benchMimalloc(benchmark::State &state) {
  std::pmr::vector<dpcpp_trace::GraphEvent> graph(
      dpcpp_trace::makeMiResource<dpcpp_trace::GraphEvent>());
  for (auto _ : state) {
    dpcpp_trace::GraphEvent event;
    auto *metadata = event.add_metadata();
    metadata->set_name("device");
    metadata->set_value("SYCL Host device");
    event.set_id(10);
    event.set_type(dpcpp_trace::GraphEvent_EventType_NODE);
    graph.push_back(event);
  }
}

BENCHMARK(benchMimalloc);

static void benchArenas(benchmark::State &state) {
  google::protobuf::Arena arena;
  std::vector<dpcpp_trace::GraphEvent *> graph;
  for (auto _ : state) {
    auto *event =
        google::protobuf::Arena::CreateMessage<dpcpp_trace::GraphEvent>(&arena);
    auto *metadata =
        google::protobuf::Arena::CreateMessage<dpcpp_trace::Metadata>(&arena);
    metadata->set_name("device");
    metadata->set_value("SYCL Host device");
    event->mutable_metadata()->AddAllocated(metadata);
    event->set_id(10);
    event->set_type(dpcpp_trace::GraphEvent_EventType_NODE);
    graph.push_back(event);
  }
}

BENCHMARK(benchArenas);

static void benchArenasMimalloc(benchmark::State &state) {
  google::protobuf::Arena arena;
  std::pmr::vector<dpcpp_trace::GraphEvent *> graph(
      dpcpp_trace::makeMiResource<dpcpp_trace::GraphEvent>());
  for (auto _ : state) {
    auto *event =
        google::protobuf::Arena::CreateMessage<dpcpp_trace::GraphEvent>(&arena);
    auto *metadata =
        google::protobuf::Arena::CreateMessage<dpcpp_trace::Metadata>(&arena);
    metadata->set_name("device");
    metadata->set_value("SYCL Host device");
    event->mutable_metadata()->AddAllocated(metadata);
    event->set_id(10);
    event->set_type(dpcpp_trace::GraphEvent_EventType_NODE);
    graph.push_back(event);
  }
}

BENCHMARK(benchArenasMimalloc);
