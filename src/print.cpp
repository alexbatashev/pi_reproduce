#include "common.hpp"
#include "constants.hpp"
#include "options.hpp"
#include "pretty_printers.hpp"

#include "pi_arguments_handler.hpp"
#include <CL/sycl/detail/pi.h>
#include <fmt/core.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

template <typename DstT, typename SrcT>
DstT offset_cast(size_t offset, SrcT *ptr) {
  char *rptr = reinterpret_cast<char *>(ptr);
  return reinterpret_cast<DstT>(rptr + offset);
}

struct PerformanceSummary {
  size_t totalCalls;
  size_t maxDuration = std::numeric_limits<size_t>::min();
  size_t minDuration = std::numeric_limits<size_t>::max();
  size_t totalDuration;
};

using RecordT = std::pair<std::string, dpcpp_trace::APICall>;

static std::string getAPIName(uint32_t id) {
  sycl::detail::PiApiKind kind = static_cast<sycl::detail::PiApiKind>(id);

  switch (kind) {
#define _PI_API(api)                                                           \
  case sycl::detail::PiApiKind::api:                                           \
    return #api;
#include <CL/sycl/detail/pi.def>
#undef _PI_API
  }

  return std::string("UNKNOWN ID : ") + std::to_string(id);
}

void parseTraceFile(std::vector<RecordT> &records, std::filesystem::path path) {
  std::ifstream trace{path, std::ios::binary};

  std::string threadId = path.stem();

  while (!trace.eof()) {
    uint32_t recordSize;
    trace.read(reinterpret_cast<char *>(&recordSize), sizeof(uint32_t));

    std::string recordData;
    recordData.resize(recordSize + 1);
    trace.read(recordData.data(), recordSize);

    dpcpp_trace::APICall record;
    record.ParseFromString(recordData);

    if (trace.eof())
      break;

    records.emplace_back(threadId, std::move(record));
  }
}

static void printRecord(const RecordT &r, bool verbose) {

  if (verbose) {
    fmt::print("\n>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
    fmt::print("{:<18} : {}\n", "Thread ID", r.first);
    fmt::print("{:<18} : {}\n", "Call duration",
               r.second.time_end() - r.second.time_start());
    fmt::print("{:<18} : {}\n\n", "Captured outputs",
               r.second.small_outputs().size() +
                   r.second.mem_obj_outputs().size());
  }
  std::cout << "----> " << getAPIName(r.second.function_id()) << "(\n";
  printArgs(r.second.args());
  std::cout << ") : " << r.second.return_value() << "\n";

  if (verbose) {
    fmt::print("\n<~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  }
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

void printImageDesc(std::filesystem::path path) {
  std::ifstream is{path, std::ios::binary};

  const auto readInt = [&is](auto &num) {
    is.read(reinterpret_cast<char *>(&num),
            sizeof(std::decay_t<decltype(num)>));
  };

  fmt::print("Image desc: {}\n", path.string());

  uint16_t version;
  readInt(version);
  fmt::print("Version: {}\n", version);

  uint8_t kind;
  readInt(kind);
  fmt::print("Kind: {}\n", kind);

  uint8_t format;
  readInt(format);
  if (format == PI_DEVICE_BINARY_TYPE_SPIRV)
    fmt::print("Format: SPIR-V\n");
  else if (format == PI_DEVICE_BINARY_TYPE_NATIVE)
    fmt::print("Format: Native\n");
  else if (format == PI_DEVICE_BINARY_TYPE_LLVMIR_BITCODE)
    fmt::print("Format: LLVM IR Bitcode\n");
  else if (format == PI_DEVICE_BINARY_TYPE_NONE)
    fmt::print("Format: None\n");

  std::array<char, 8192> buf;
  size_t length;

  readInt(length);
  is.read(buf.data(), length);

  fmt::print("Device target: {}\n", std::string_view(buf.data(), length - 1));

  readInt(length);
  is.read(buf.data(), length);

  fmt::print("Compile options: {}\n", std::string_view(buf.data(), length - 1));

  readInt(length);
  is.read(buf.data(), length);

  fmt::print("Link options: {}\n", std::string_view(buf.data(), length - 1));

  size_t numOffloadEntries;
  readInt(numOffloadEntries);

  fmt::print("Offload entries [{}]:\n", numOffloadEntries);
  for (size_t i = 0; i < numOffloadEntries; i++) {
    size_t addr;
    readInt(addr);
    fmt::print("{:>5}Address: {}\n", " ", addr);

    readInt(length);
    is.read(buf.data(), length);

    fmt::print("{:>5}Name: {}\n", " ",
               std::string_view(buf.data(), length - 1));

    size_t size;
    readInt(size);
    fmt::print("{:>5}Size: {}\n\n", " ", size);
  }

  size_t numPropSets;
  readInt(numPropSets);
  fmt::print("Property sets [{}]:\n", numPropSets);
  for (size_t i = 0; i < numPropSets; ++i) {
    readInt(length);
    is.read(buf.data(), length);

    fmt::print("{:>5}Name: {}\n", " ",
               std::string_view(buf.data(), length - 1));

    size_t numProperties;
    readInt(numProperties);
    fmt::print("{:>5}Properties [{}]:\n", " ", numProperties);

    for (size_t j = 0; j < numProperties; j++) {
      readInt(length);
      is.read(buf.data(), length);

      fmt::print("{:>10}Name: {}\n", " ",
                 std::string_view(buf.data(), length - 1));

      size_t valAddr;
      readInt(valAddr);

      fmt::print("{:>10}Value address: {}\n", " ", valAddr);

      int type;
      readInt(type);
      switch (static_cast<pi_property_type>(type)) {
      case PI_PROPERTY_TYPE_UNKNOWN:
        fmt::print("{:>10}Type: unknown\n", " ");
        break;
      case PI_PROPERTY_TYPE_UINT32:
        fmt::print("{:>10}Type: uint32\n", " ");
        break;
      case PI_PROPERTY_TYPE_BYTE_ARRAY:
        fmt::print("{:>10}Type: byte array\n", " ");
        break;
      case PI_PROPERTY_TYPE_STRING:
        fmt::print("{:>10}Type: string\n", " ");
        break;
      }

      size_t size;
      readInt(size);
      fmt::print("{:>10}Value size: {}\n", " ", size);
    }
  }
  fmt::print("\n");
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

  std::vector<RecordT> records;

  fmt::print("Binary images:\n\n");
  for (auto &de : std::filesystem::directory_iterator(opts.input())) {
    if (de.path().extension().string() == ".desc") {
      printImageDesc(de.path());
    }
  }
  fmt::print("------------------------------------------\n");
  for (auto &de : std::filesystem::directory_iterator(opts.input())) {
    if (de.path().extension().string() == kPiTraceExt) {
      parseTraceFile(records, de.path());
    }
  }

  std::map<uint32_t, PerformanceSummary> perfMap;

  const auto collectPerf = [&perfMap](const RecordT &r) {
    if (perfMap.count(r.second.function_id()) == 0) {
      perfMap[r.second.function_id()] = PerformanceSummary{};
    }
    PerformanceSummary &ps = perfMap[r.second.function_id()];
    ps.totalCalls++;
    uint64_t duration = r.second.time_end() - r.second.time_start();
    ps.totalDuration += duration;
    ps.maxDuration = std::max(ps.maxDuration, duration);
    ps.minDuration = std::min(ps.minDuration, duration);
  };

  if (opts.print_group() == options::print_group_by::thread) {
    std::string lastThread = "";
    for (auto &r : records) {
      if (lastThread != r.first) {
        if (!lastThread.empty()) {
          std::cout << "~END THREAD : " << lastThread << "\n\n";
        }
        lastThread = r.first;
        std::cout << "~START THREAD : " << lastThread << "\n\n";
      }
      printRecord(r, opts.verbose());
      collectPerf(r);
    }
    if (opts.performance_summary()) {
      printPerformanceSummary(perfMap);
    }
    perfMap.clear();
    std::cout << "~END THREAD : " << lastThread << "\n\n";
  } else {
    std::sort(records.begin(), records.end(),
              [](const RecordT &a, const RecordT &b) {
                return a.second.time_start() < b.second.time_start();
              });
    for (auto &r : records) {
      std::cout << "THREAD : " << r.first << "\n";
      printRecord(r, opts.verbose());
      collectPerf(r);
    }
    if (opts.performance_summary()) {
      printPerformanceSummary(perfMap);
    }
  }
}
