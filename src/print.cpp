#include "common.hpp"

#include "options.hpp"
#include "pi_arguments_handler.hpp"
#include <CL/sycl/detail/pi.h>
#include <detail/plugin_printers.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

template <typename DstT, typename SrcT>
DstT offset_cast(size_t offset, SrcT *ptr) {
  char *rptr = reinterpret_cast<char *>(ptr);
  return reinterpret_cast<DstT>(rptr + offset);
}

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

void printTrace(const options &opts) {
  if (!std::filesystem::exists(opts.input())) {
    std::cerr << "Input path does not exist: " << opts.input() << "\n";
    exit(-1);
  }
  if (!std::filesystem::is_directory(opts.input())) {
    std::cerr << "Input path is not a directory: " << opts.input() << "\n";
    exit(-1);
  }
  auto path = opts.input() / "trace.data";
  if (!std::filesystem::exists(path)) {
    std::cerr << "Trace file not found: " << path << "\n";
    exit(-1);
  }
  std::ifstream trace{path, std::ios::binary};

  sycl::xpti_helpers::PiArgumentsHandler argHandler;

#define _PI_API(api, ...)                                                      \
  argHandler.set##_##api([](auto &&...Args) {                                  \
    std::cout << "---> " << #api << "("                                        \
              << "\n";                                                         \
    sycl::detail::pi::printArgs(Args...);                                      \
    std::cout << ") ---> ";                                                    \
  });
#include <CL/sycl/detail/pi.def>
#undef _PI_API

  while (!trace.eof()) {
    uint32_t functionId;
    size_t numOutputs;
    size_t totalSize;
    pi_result result;

    trace.read(reinterpret_cast<char *>(&functionId), sizeof(uint32_t));
    trace.read(reinterpret_cast<char *>(&numOutputs), sizeof(size_t));
    trace.read(reinterpret_cast<char *>(&totalSize), sizeof(size_t));

    // 8KiB should be more than enough for all PI calls arguments.
    std::array<unsigned char, 8192> argumentsData;

    trace.read(reinterpret_cast<char *>(argumentsData.data()), totalSize);

    for (size_t i = 0; i < numOutputs; i++) {
      size_t dataSize;
      trace.read(reinterpret_cast<char *>(&dataSize), sizeof(size_t));
      trace.seekg(dataSize, std::ios_base::cur); // skip output for now
    }

    trace.read(reinterpret_cast<char *>(&result), sizeof(pi_result));

    using namespace sycl::detail;
    if (functionId == static_cast<uint32_t>(PiApiKind::piProgramBuild)) {
      printProgramBuild(argumentsData.data());
    } else if (functionId == static_cast<uint32_t>(PiApiKind::piKernelCreate)) {
      printKernelCreate(argumentsData.data());
    } else {
      argHandler.handle(functionId, argumentsData.data());
    }
    std::cout << result << "\n";
  }
}
