#include "record_handler.hpp"
#include "write_utils.hpp"
#include "constants.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>

static std::atomic_bool GBinariesCollected = false;
static std::mutex GBinariesMutex;

void handleSelectBinary(std::ostream &os, sycl::detail::XPTIPluginInfo,
                        std::optional<pi_result>, pi_device device,
                        pi_device_binary *binaries, pi_uint32 numBinaries,
                        pi_uint32 *selectedBinary) {
  if (!GBinariesCollected) {
    std::lock_guard lock{GBinariesMutex};

    if (GBinariesCollected)
      return;

    std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};
    GBinariesCollected = true;
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
      std::ofstream bos{binPath, std::ofstream::binary};
      size_t binarySize =
          std::distance(binaries[i]->BinaryStart, binaries[i]->BinaryEnd);
      bos.write(reinterpret_cast<const char *>(binaries[i]->BinaryStart),
                binarySize);
      bos.close();
    }
  }

  writeWithOutput(os, 1, device, binaries, numBinaries, selectedBinary);

  size_t outSize = sizeof(pi_uint32 *);
  os.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(selectedBinary), sizeof(outSize));
}

void handleProgramBuild(std::ostream &os, sycl::detail::XPTIPluginInfo,
                        std::optional<pi_result>, pi_program prog,
                        pi_uint32 numDevices, const pi_device *devices,
                        const char *opts,
                        void (*pfn_notify)(pi_program program, void *user_data),
                        void *user_data) {
  size_t totalSize = sizeof(pi_program) + sizeof(pi_uint32) +
                     3 * sizeof(size_t) + std::strlen(opts) + 1;
  size_t numOutputs = 0;

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));
  write(os, prog, numDevices, devices, opts, pfn_notify, user_data);
}

void handleKernelCreate(std::ostream &os, sycl::detail::XPTIPluginInfo,
                        std::optional<pi_result>, pi_program prog,
                        const char *kernelName, pi_kernel *retKernel) {
  size_t totalSize =
      sizeof(pi_program) + sizeof(pi_kernel *) + strlen(kernelName) + 1;
  size_t numOutputs = 0;

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));
  write(os, prog, kernelName, retKernel);
}

void handlePlatformsGet(std::ostream &os, sycl::detail::XPTIPluginInfo,
                        std::optional<pi_result>, pi_uint32 numEntries,
                        pi_platform *platforms, pi_uint32 *numPlatforms) {
  size_t totalSize = sizeof(pi_uint32) + 2 * sizeof(void *);
  size_t numOutputs = (numPlatforms == nullptr) ? 0 : 1;

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));

  write(os, numEntries, platforms, numPlatforms);

  if (numPlatforms != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    os.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    os.write(reinterpret_cast<const char *>(numPlatforms), outSize);
  }
}

void handleDevicesGet(std::ostream &os, sycl::detail::XPTIPluginInfo,
                      std::optional<pi_result>, pi_platform platform,
                      pi_device_type type, pi_uint32 numEntries,
                      pi_device *devs, pi_uint32 *numDevices) {
  size_t totalSize = sizeof(pi_platform) + sizeof(pi_device_type) +
                     sizeof(pi_uint32) + 2 * sizeof(void *);
  size_t numOutputs = (numDevices == nullptr) ? 0 : 1;

  writeWithOutput(os, numOutputs, platform, type, numEntries, devs, numDevices);

  if (numDevices != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    os.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    os.write(reinterpret_cast<const char *>(numDevices), outSize);
  }
}

void handleEnqueueMemBufferMap(std::ostream &os, bool writeMemObj,
                               sycl::detail::XPTIPluginInfo Plugin,
                               std::optional<pi_result>, pi_queue command_queue,
                               pi_mem buffer, pi_bool blocking_map,
                               pi_map_flags map_flags, size_t offset,
                               size_t size, pi_uint32 num_events_in_wait_list,
                               const pi_event *event_wait_list, pi_event *event,
                               void **ret_map) {
  // Wait for map to finish. There's no need to wait, if user asked to skip mem
  // objects. This will provide more accurate performance statistics.
  if (writeMemObj)
    Plugin.plugin.PiFunctionTable.piEventsWait(1, event);

  size_t numOutputs = writeMemObj ? 1 : 0;

  writeWithOutput(os, numOutputs, command_queue, buffer, blocking_map,
                  map_flags, offset, size, num_events_in_wait_list,
                  event_wait_list, event, ret_map);

  if (writeMemObj) {
    os.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    os.write(static_cast<const char *>(*ret_map), size);
  }
}

void handleEnqueueMemBufferRead(std::ostream &os, bool writeMemObj,
                                sycl::detail::XPTIPluginInfo Plugin,
                                std::optional<pi_result>, pi_queue queue,
                                pi_mem buffer, pi_bool blocking_read,
                                size_t offset, size_t size, void *ptr,
                                pi_uint32 num_events_in_wait_list,
                                const pi_event *event_wait_list,
                                pi_event *event) {

  // Wait for read to finish. There's no need to wait, if user asked to skip mem
  // objects. This will provide more accurate performance statistics.
  if (writeMemObj)
    Plugin.plugin.PiFunctionTable.piEventsWait(1, event);

  size_t numOutputs = writeMemObj ? 1 : 0;

  writeWithOutput(os, numOutputs, queue, buffer, blocking_read, offset, size,
                  ptr, num_events_in_wait_list, event_wait_list, event);

  if (writeMemObj) {
    os.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    os.write(static_cast<const char *>(ptr), size);
  }
}

void handleKernelGetGroupInfo(std::ostream &os, sycl::detail::XPTIPluginInfo,
                              std::optional<pi_result>, pi_kernel kernel,
                              pi_device device, pi_kernel_group_info param_name,
                              size_t Size, void *Value, size_t *RetSize) {
  size_t TotalSize = sizeof(pi_kernel) + sizeof(pi_device) +
                     sizeof(pi_kernel_group_info) + 3 * sizeof(size_t);
  size_t numOutputs = 1;
  writeWithOutput(os, numOutputs, kernel, device, param_name, Size, Value,
                  RetSize);

  if (Value != nullptr) {
    os.write(reinterpret_cast<const char *>(&Size), sizeof(size_t));
    os.write(static_cast<const char *>(Value), Size);
  }
  if (RetSize != nullptr) {
    size_t outSize = sizeof(size_t);
    os.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    os.write(reinterpret_cast<const char *>(RetSize), outSize);
  }
}
RecordHandler::RecordHandler(
    std::unique_ptr<std::ostream> os,
    std::chrono::time_point<std::chrono::steady_clock> timestamp,
    bool skipMemObjects)
    : mOS(std::move(os)), mStartTime(timestamp),
      mSkipMemObjects(skipMemObjects) {

#define _PI_API(api)                                                           \
  mArgHandler.set##_##api(                                                     \
      [this](auto &&...Args) { writeSimple(*mOS, Args...); });
#include <CL/sycl/detail/pi.def>
#undef _PI_API

  const auto wrap = [this](auto func) {
    return [this, func](auto &&...args) { std::invoke(func, *mOS, args...); };
  };

  const auto wrapMem = [this](auto func) {
    return [this, func](auto &&...args) {
      std::invoke(func, *mOS, mSkipMemObjects, args...);
    };
  };

  mArgHandler.set_piextDeviceSelectBinary(wrap(handleSelectBinary));
  mArgHandler.set_piProgramBuild(wrap(handleProgramBuild));
  mArgHandler.set_piKernelCreate(wrap(handleKernelCreate));
  mArgHandler.set_piPlatformsGet(wrap(handlePlatformsGet));
  mArgHandler.set_piDevicesGet(wrap(handleDevicesGet));
  mArgHandler.set_piEnqueueMemBufferMap(wrapMem(handleEnqueueMemBufferMap));
  mArgHandler.set_piEnqueueMemBufferRead(wrapMem(handleEnqueueMemBufferRead));
  mArgHandler.set_piPlatformGetInfo(
      [this](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
             auto &&...Args) { writeWithInfo(*mOS, Args...); });
  mArgHandler.set_piDeviceGetInfo(
      [this](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
             auto &&...Args) { writeWithInfo(*mOS, Args...); });
  mArgHandler.set_piContextGetInfo(
      [this](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
             auto &&...Args) { writeWithInfo(*mOS, Args...); });
  mArgHandler.set_piKernelGetInfo(
      [this](sycl::detail::XPTIPluginInfo, std::optional<pi_result>,
             auto &&...Args) { writeWithInfo(*mOS, Args...); });
  mArgHandler.set_piKernelGetGroupInfo(wrap(handleKernelGetGroupInfo));
}

void RecordHandler::handle(uint32_t funcId,
                           const sycl::detail::XPTIPluginInfo &plugin,
                           std::optional<pi_result> result, void *data) {
  size_t numInputs = 0; // unused for now.
  mOS->write(reinterpret_cast<const char *>(&numInputs), sizeof(size_t));
  mArgHandler.handle(funcId, plugin, result, data);

  mOS->write(reinterpret_cast<const char *>(&result.value()),
             sizeof(pi_result));
}

void RecordHandler::flush() { mOS->flush(); }

void RecordHandler::writeHeader(uint32_t funcId, uint8_t backend) {
  mOS->write(reinterpret_cast<const char *>(&funcId), sizeof(uint32_t));
  mOS->write(reinterpret_cast<const char *>(&backend), sizeof(uint8_t));
}

void RecordHandler::timestamp() {
  const auto end = std::chrono::steady_clock::now();
  uint64_t endPoint =
      std::chrono::duration_cast<std::chrono::microseconds>(end - mStartTime)
          .count();
  mOS->write(reinterpret_cast<const char *>(&endPoint), sizeof(uint64_t));
}
