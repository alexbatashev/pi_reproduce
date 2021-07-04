#include <CL/sycl/detail/xpti_plugin_info.hpp>

#include "pi_arguments_handler.hpp"
#include "xpti_trace_framework.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

static uint8_t GStreamID = 0;
std::mutex GIOMutex;

bool binariesCollected = false;

std::ofstream RecordData;

sycl::xpti_helpers::PiArgumentsHandler ArgHandler;

template <typename... Ts> struct write_helper;

template <typename T, typename... Rest> struct write_helper<T, Rest...> {
  static void write(T Data, Rest... Args) {
    RecordData.write(reinterpret_cast<const char *>(&Data), sizeof(T));
    write_helper<Rest...>::write(Args...);
  }
};

template <typename T> struct write_helper<T> {
  static void write(T Data) {
    RecordData.write(reinterpret_cast<const char *>(&Data), sizeof(T));
  }
};

template <typename... Ts> void writeArgs(Ts &&...args) {
  write_helper<Ts...>::write(args...);
}

template <typename... Ts> void saveDefaultArgs(Ts... Args) {
  size_t TotalSize = (sizeof(Ts) + ...);
  size_t NumOutputs = 0;

  RecordData.write(reinterpret_cast<const char *>(&NumOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&TotalSize), sizeof(size_t));
  write_helper<Ts...>::write(Args...);
}

template <typename T1, typename T2>
void saveGetInfo(T1 Obj, T2 Param, size_t Size, void *Value, size_t *RetSize) {
  size_t TotalSize = sizeof(T1) + sizeof(T2) + 3 * sizeof(size_t);
  size_t NumOutputs = 1;

  RecordData.write(reinterpret_cast<const char *>(&NumOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&TotalSize), sizeof(size_t));
  write_helper<T1, T2, size_t, void *, size_t *>::write(Obj, Param, Size, Value,
                                                        RetSize);

  if (Value != nullptr) {
    RecordData.write(reinterpret_cast<const char *>(&Size), sizeof(size_t));
    RecordData.write(static_cast<const char *>(Value), Size);
  }
  if (RetSize != nullptr) {
    size_t outSize = sizeof(size_t);
    RecordData.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(RetSize), outSize);
  }
}

void handleSelectBinary(sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
                        pi_device device, pi_device_binary *binaries,
                        pi_uint32 numBinaries, pi_uint32 *selectedBinary) {
  if (!binariesCollected) {
    std::filesystem::path outDir{std::getenv("PI_REPRODUCE_TRACE_PATH")};
    binariesCollected = true;
    for (pi_uint32 i = 0; i < numBinaries; i++) {
      std::string extension;
      if (binaries[i]->Format == PI_DEVICE_BINARY_TYPE_SPIRV)
        extension = ".spv";
      else if (binaries[i]->Format == PI_DEVICE_BINARY_TYPE_NATIVE)
        extension = ".bin";
      else if (binaries[i]->Format == PI_DEVICE_BINARY_TYPE_LLVMIR_BITCODE)
        extension = ".bc";
      else if (binaries[i]->Format == PI_DEVICE_BINARY_TYPE_NONE)
        extension = ".none";

      auto binPath =
          outDir / (std::to_string(i) + "_" +
                    std::string(binaries[i]->DeviceTargetSpec) + extension);
      std::ofstream os{binPath, std::ofstream::binary};
      size_t binarySize =
          std::distance(binaries[i]->BinaryStart, binaries[i]->BinaryEnd);
      os.write(reinterpret_cast<const char *>(binaries[i]->BinaryStart),
               binarySize);
      os.close();
    }
  }

  size_t TotalSize = sizeof(pi_device) + 2 * sizeof(void *) + sizeof(pi_uint32);
  size_t NumOutputs = 1;

  RecordData.write(reinterpret_cast<const char *>(&NumOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&TotalSize), sizeof(size_t));
  write_helper<pi_device, pi_device_binary *, pi_uint32, pi_uint32 *>::write(
      device, binaries, numBinaries, selectedBinary);

  size_t outSize = sizeof(pi_uint32 *);
  RecordData.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(selectedBinary),
                   sizeof(outSize));
}

void handleProgramBuild(sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
                        pi_program prog, pi_uint32 numDevices,
                        const pi_device *devices, const char *opts,
                        void (*pfn_notify)(pi_program program, void *user_data),
                        void *user_data) {
  size_t totalSize = sizeof(pi_program) + sizeof(pi_uint32) +
                     3 * sizeof(size_t) + strlen(opts) + 1;
  size_t numOutputs = 0;

  RecordData.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));
  write_helper<pi_program, pi_uint32, const pi_device *>::write(
      prog, numDevices, devices);

  RecordData.write(opts, strlen(opts));
  RecordData.put('\0');

  write_helper<decltype(pfn_notify), void *>::write(pfn_notify, user_data);
}

void handleKernelCreate(sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
                        pi_program prog, const char *kernelName,
                        pi_kernel *retKernel) {
  size_t totalSize =
      sizeof(pi_program) + sizeof(pi_kernel *) + strlen(kernelName) + 1;
  size_t numOutputs = 0;

  RecordData.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));
  write_helper<pi_program>::write(prog);

  RecordData.write(kernelName, strlen(kernelName));
  RecordData.put('\0');

  write_helper<pi_kernel *>::write(retKernel);
}

void handlePlatformsGet(sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
                        pi_uint32 numEntries, pi_platform *platforms,
                        pi_uint32 *numPlatforms) {
  size_t totalSize = sizeof(pi_uint32) + 2 * sizeof(void *);
  size_t numOutputs = (numPlatforms == nullptr) ? 0 : 1;

  RecordData.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));

  write_helper<pi_uint32, pi_platform *, pi_uint32 *>::write(
      numEntries, platforms, numPlatforms);

  if (numPlatforms != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    RecordData.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(numPlatforms), outSize);
  }
}

void handleDevicesGet(sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
                      pi_platform platform, pi_device_type type,
                      pi_uint32 numEntries, pi_device *devs,
                      pi_uint32 *numDevices) {
  size_t totalSize = sizeof(pi_platform) + sizeof(pi_device_type) +
                     sizeof(pi_uint32) + 2 * sizeof(void *);
  size_t numOutputs = (numDevices == nullptr) ? 0 : 1;

  RecordData.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));

  write_helper<pi_platform, pi_device_type, pi_uint32, pi_device *,
               pi_uint32 *>::write(platform, type, numEntries, devs,
                                   numDevices);

  if (numDevices != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    RecordData.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(numDevices), outSize);
  }
}

void handleEnqueueMemBufferMap(sycl::detail::XPTIPluginInfo Plugin,
                               std::optional<pi_result>, pi_queue command_queue,
                               pi_mem buffer, pi_bool blocking_map,
                               pi_map_flags map_flags, size_t offset,
                               size_t size, pi_uint32 num_events_in_wait_list,
                               const pi_event *event_wait_list, pi_event *event,
                               void **ret_map) {
  // Wait for map to finish
  Plugin.plugin.PiFunctionTable.piEventsWait(1, event);

  size_t numOutputs = 1;

  const auto saveFunc = [numOutputs](auto... args) {
    size_t totalSize = (sizeof(decltype(args)) + ...);

    RecordData.write(reinterpret_cast<const char *>(&numOutputs),
                     sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(&totalSize),
                     sizeof(size_t));

    write_helper<decltype(args)...>::write(args...);
  };
  saveFunc(command_queue, buffer, blocking_map, map_flags, offset, size,
           num_events_in_wait_list, event_wait_list, event, ret_map);

  RecordData.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
  RecordData.write(static_cast<const char *>(*ret_map), size);
}

void handleEnqueueMemBufferRead(sycl::detail::XPTIPluginInfo Plugin,
                                std::optional<pi_result>, pi_queue queue,
                                pi_mem buffer, pi_bool blocking_read,
                                size_t offset, size_t size, void *ptr,
                                pi_uint32 num_events_in_wait_list,
                                const pi_event *event_wait_list,
                                pi_event *event) {

  Plugin.plugin.PiFunctionTable.piEventsWait(1, event);

  size_t numOutputs = 1;

  const auto saveFunc = [numOutputs](auto... args) {
    size_t totalSize = (sizeof(decltype(args)) + ...);

    RecordData.write(reinterpret_cast<const char *>(&numOutputs),
                     sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(&totalSize),
                     sizeof(size_t));

    write_helper<decltype(args)...>::write(args...);
  };
  saveFunc(queue, buffer, blocking_read, offset, size, ptr,
           num_events_in_wait_list, event_wait_list, event);

  RecordData.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
  RecordData.write(static_cast<const char *>(ptr), size);
}

void handleKernelGetGroupInfo(sycl::detail::XPTIPluginInfo,
                              std::optional<pi_result>, pi_kernel kernel,
                              pi_device device, pi_kernel_group_info param_name,
                              size_t Size, void *Value, size_t *RetSize) {
  size_t TotalSize = sizeof(pi_kernel) + sizeof(pi_device) +
                     sizeof(pi_kernel_group_info) + 3 * sizeof(size_t);
  size_t NumOutputs = 1;

  RecordData.write(reinterpret_cast<const char *>(&NumOutputs), sizeof(size_t));
  RecordData.write(reinterpret_cast<const char *>(&TotalSize), sizeof(size_t));
  writeArgs(kernel, device, param_name, Size, Value, RetSize);

  if (Value != nullptr) {
    RecordData.write(reinterpret_cast<const char *>(&Size), sizeof(size_t));
    RecordData.write(static_cast<const char *>(Value), Size);
  }
  if (RetSize != nullptr) {
    size_t outSize = sizeof(size_t);
    RecordData.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    RecordData.write(reinterpret_cast<const char *>(RetSize), outSize);
  }
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

#define _PI_API(api, ...)                                                      \
  ArgHandler.set##_##api([](auto &&...Args) { saveDefaultArgs(Args...); });
#include <CL/sycl/detail/pi.def>
#undef _PI_API

    ArgHandler.set_piextDeviceSelectBinary(handleSelectBinary);
    ArgHandler.set_piProgramBuild(handleProgramBuild);
    ArgHandler.set_piKernelCreate(handleKernelCreate);
    ArgHandler.set_piPlatformsGet(handlePlatformsGet);
    ArgHandler.set_piDevicesGet(handleDevicesGet);
    ArgHandler.set_piEnqueueMemBufferMap(handleEnqueueMemBufferMap);
    ArgHandler.set_piEnqueueMemBufferRead(handleEnqueueMemBufferRead);
    ArgHandler.set_piPlatformGetInfo(
        [](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
           auto &&...Args) { saveGetInfo(Args...); });
    ArgHandler.set_piDeviceGetInfo(
        [](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
           auto &&...Args) { saveGetInfo(Args...); });
    ArgHandler.set_piContextGetInfo(
        [](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
           auto &&...Args) { saveGetInfo(Args...); });
    ArgHandler.set_piKernelGetInfo(
        [](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
           auto &&...Args) { saveGetInfo(Args...); });
    ArgHandler.set_piKernelGetGroupInfo(handleKernelGetGroupInfo);

    std::filesystem::path outDir{std::getenv("PI_REPRODUCE_TRACE_PATH")};
    RecordData =
        std::ofstream(outDir / "trace.data", std::ios::out | std::ios::binary);
  }
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *stream_name) {
  if (std::string_view(stream_name) == "sycl.pi.arg") {
    RecordData.close();
  }
}

XPTI_CALLBACK_API void tpCallback(uint16_t TraceType,
                                  xpti::trace_event_data_t *Parent,
                                  xpti::trace_event_data_t *Event,
                                  uint64_t Instance, const void *UserData) {
  auto Type = static_cast<xpti::trace_point_type_t>(TraceType);
  if (Type == xpti::trace_point_type_t::function_with_args_end) {
    // Lock while we print information
    std::lock_guard<std::mutex> Lock(GIOMutex);

    const auto *Data =
        static_cast<const xpti::function_with_args_t *>(UserData);

    RecordData.write(reinterpret_cast<const char *>(&Data->function_id),
                     sizeof(uint32_t));

    const auto *Plugin =
        static_cast<sycl::detail::XPTIPluginInfo *>(Data->user_data);
    const pi_result Result = *static_cast<pi_result *>(Data->ret_data);
    ArgHandler.handle(Data->function_id, *Plugin, Result, Data->args_data);

    RecordData.write(reinterpret_cast<const char *>(Data->ret_data),
                     sizeof(pi_result));
  }
}
