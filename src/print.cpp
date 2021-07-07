#include "common.hpp"
#include "options.hpp"
#include "pretty_printers.hpp"

#include "pi_arguments_handler.hpp"
#include <CL/sycl/detail/pi.h>
#include <CL/sycl/detail/xpti_plugin_info.hpp>
#include <fmt/core.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

template <typename DstT, typename SrcT>
DstT offset_cast(size_t offset, SrcT *ptr) {
  char *rptr = reinterpret_cast<char *>(ptr);
  return reinterpret_cast<DstT>(rptr + offset);
} // TODO replace with pretty printers as well
static void printProgramBuild(void *data) {
  size_t offset = 0;
  std::cout << "---> piProgramBuild(\n";
  std::cout << "        <pi_program> : "
            << *offset_cast<pi_program *>(offset, data) << "\n";
  offset += sizeof(pi_program);
  std::cout << "        <pi_uint32> : "
            << *offset_cast<pi_uint32 *>(offset, data) << "\n";
  offset += sizeof(pi_uint32);
  std::cout << "        <const pi_device*> : "
            << *offset_cast<const pi_device **>(offset, data) << "\n";
  offset += sizeof(const pi_device *);
  std::cout << "        <const char*> : "
            << offset_cast<const char *>(offset, data) << "\n";
  offset += strlen(offset_cast<const char *>(offset, data)) + 1;
  std::cout << "        <unknown> : " << *offset_cast<void **>(offset, data)
            << "\n";
  offset += sizeof(void *);
  std::cout << "        <void *> : " << *offset_cast<void **>(offset, data)
            << "\n";
  std::cout << ") ---> ";
}

static void printKernelCreate(void *data) {
  size_t offset = 0;
  std::cout << "---> piKernelCreate(\n";
  std::cout << "        <pi_program> : "
            << *offset_cast<pi_program *>(offset, data) << "\n";
  offset += sizeof(pi_program);
  std::cout << "        <const char*> : "
            << offset_cast<const char *>(offset, data) << "\n";
  offset += strlen(offset_cast<const char *>(offset, data)) + 1;
  std::cout << "        <pi_kernel> : "
            << *offset_cast<pi_kernel **>(offset, data) << "\n";
  std::cout << ") ---> ";
}

struct Record {
  uint32_t functionId;
  uint8_t backend;
  size_t numInputs;
  size_t numOutputs;
  pi_result result;
  std::vector<char> argsData;
  std::string threadId;
  uint64_t start;
  uint64_t end;
};

struct PerformanceSummary {
  size_t totalCalls;
  size_t maxDuration = std::numeric_limits<size_t>::min();
  size_t minDuration = std::numeric_limits<size_t>::max();
  size_t totalDuration;
};

void parseTraceFile(std::vector<Record> &records, std::filesystem::path path) {
  std::ifstream trace{path, std::ios::binary};
  while (!trace.eof()) {
    uint32_t functionId;
    uint8_t backend;
    uint64_t start;
    uint64_t end;
    size_t numInputs;
    size_t numOutputs;
    size_t totalSize;
    pi_result result;

    trace.read(reinterpret_cast<char *>(&functionId), sizeof(uint32_t));
    trace.read(reinterpret_cast<char *>(&backend), sizeof(uint8_t));
    trace.read(reinterpret_cast<char *>(&start), sizeof(uint64_t));
    trace.read(reinterpret_cast<char *>(&end), sizeof(uint64_t));
    trace.read(reinterpret_cast<char *>(&numInputs), sizeof(size_t));
    trace.read(reinterpret_cast<char *>(&numOutputs), sizeof(size_t));
    trace.read(reinterpret_cast<char *>(&totalSize), sizeof(size_t));

    Record record;
    record.functionId = functionId;
    record.backend = backend;
    record.start = start;
    record.end = end;
    record.threadId = path.stem().filename().string();
    record.numOutputs = numOutputs;

    record.argsData.resize(totalSize);
    trace.read(reinterpret_cast<char *>(record.argsData.data()), totalSize);

    for (size_t i = 0; i < numOutputs; i++) {
      size_t dataSize;
      trace.read(reinterpret_cast<char *>(&dataSize), sizeof(size_t));
      trace.seekg(dataSize, std::ios_base::cur); // skip output for now
    }

    trace.read(reinterpret_cast<char *>(&result), sizeof(pi_result));
    record.result = result;

    records.push_back(record);
  }
}

static std::string getBackend(uint8_t ub) {
  sycl::backend b = static_cast<sycl::backend>(ub);
  switch (b) {
  case sycl::backend::opencl:
    return "OpenCL";
  case sycl::backend::level_zero:
    return "Level Zero";
  case sycl::backend::cuda:
    return "CUDA";
  case sycl::backend::rocm:
    return "ROCm";
  default:
    return "unknown";
  }
}

static void printRecord(sycl::xpti_helpers::PiArgumentsHandler &argHandler,
                        Record &r, bool verbose) {
  if (verbose) {
    fmt::print("\n>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
    fmt::print("{:<18} : {}\n", "Thread ID", r.threadId);
    fmt::print("{:<18} : {}\n", "Backend", getBackend(r.backend));
    fmt::print("{:<18} : {}\n", "Call duration", r.end - r.start);
    fmt::print("{:<18} : {}\n\n", "Captured outputs", r.numOutputs);
  }
  using namespace sycl::detail;
  if (r.functionId == static_cast<uint32_t>(PiApiKind::piProgramBuild)) {
    printProgramBuild(r.argsData.data());
  } else if (r.functionId == static_cast<uint32_t>(PiApiKind::piKernelCreate)) {
    printKernelCreate(r.argsData.data());
  } else {
    XPTIPluginInfo plugin;
    argHandler.handle(r.functionId, plugin, r.result, r.argsData.data());
  }
  std::cout << r.result << "\n";

  if (verbose) {
    fmt::print("\n<~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  }
}

static std::string getAPIName(uint32_t id) {
  sycl::detail::PiApiKind kind = static_cast<sycl::detail::PiApiKind>(id);

  switch (kind) {
#define _PI_API(api, ...)                                                      \
  case sycl::detail::PiApiKind::api:                                           \
    return #api;
#include <CL/sycl/detail/pi.def>
#undef _PI_API
  }

  return "UNKNOWN";
}

static void
printPerformanceSummary(std::map<uint32_t, PerformanceSummary> &perfMap) {
  fmt::print("Performance summary:\n");
  fmt::print("{:>35} | {:^15} | {:^15} | {:^15} | {:^15} |\n", " ", "Calls",
             "Min time", "Max time", "Avg time");
  for (auto &pair : perfMap) {
    fmt::print("{:>35} | {:15} | {:13}us | {:13}us | {:13}us |\n",
               getAPIName(pair.first), pair.second.totalCalls,
               pair.second.minDuration, pair.second.maxDuration,
               pair.second.totalDuration / pair.second.totalCalls);
  }
}

void printTrace(const options &opts) {
  if (!std::filesystem::exists(opts.input())) {
    std::cerr << "Input path does not exist: " << opts.input() << "\n";
    exit(-1);
  }
  if (!std::filesystem::is_directory(opts.input())) {
    std::cerr << "Input path is not a directory: " << opts.input() << "\n";
    exit(-1);
  }

  sycl::xpti_helpers::PiArgumentsHandler argHandler;

#define _PI_API(api, ...)                                                      \
  argHandler.set##_##api([](sycl::detail::XPTIPluginInfo,                      \
                            std::optional<pi_result>, auto... Args) {          \
    std::cout << "---> " << #api << "("                                        \
              << "\n";                                                         \
    printArgs<sycl::detail::PiApiKind::api>(Args...);                          \
    std::cout << ") ---> ";                                                    \
  });
#include <CL/sycl/detail/pi.def>
#undef _PI_API

  std::vector<Record> records;

  for (auto &de : std::filesystem::directory_iterator(opts.input())) {
    if (de.path().extension().string() == ".trace") {
      parseTraceFile(records, de.path());
    }
  }

  std::map<uint32_t, PerformanceSummary> perfMap;

  const auto collectPerf = [&perfMap](const Record &r) {
    if (perfMap.count(r.functionId) == 0) {
      perfMap[r.functionId] = PerformanceSummary{};
    }
    PerformanceSummary &ps = perfMap[r.functionId];
    ps.totalCalls++;
    ps.totalDuration += r.end - r.start;
    ps.maxDuration = std::max(ps.maxDuration, r.end - r.start);
    ps.minDuration = std::min(ps.minDuration, r.end - r.start);
  };

  if (opts.print_group() == options::print_group_by::thread) {
    std::string lastThread = "";
    for (auto &r : records) {
      if (lastThread != r.threadId) {
        if (!lastThread.empty()) {
          std::cout << "~END THREAD : " << lastThread << "\n\n";
        }
        lastThread = r.threadId;
        std::cout << "~START THREAD : " << lastThread << "\n\n";
      }
      printRecord(argHandler, r, opts.verbose());
      collectPerf(r);
    }
    if (opts.performance_summary()) {
      printPerformanceSummary(perfMap);
    }
    perfMap.clear();
    std::cout << "~END THREAD : " << lastThread << "\n\n";
  } else {
    std::sort(
        records.begin(), records.end(),
        [](const Record &a, const Record &b) { return a.start < b.start; });
    for (auto &r : records) {
      std::cout << "THREAD : " << r.threadId << "\n";
      printRecord(argHandler, r, opts.verbose());
      collectPerf(r);
    }
    if (opts.performance_summary()) {
      printPerformanceSummary(perfMap);
    }
  }
}
