#include "record_handler.hpp"
#include "constants.hpp"
#include "record.pb.h"
#include "utils.hpp"
#include "write_utils.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <ostream>
#include <string>

static std::atomic_bool GBinariesCollected = false;
static std::mutex GBinariesMutex;

static void serialize(dpcpp_trace::APICall &call, std::ostream &os) {
  std::string out;
  call.SerializeToString(&out);
  uint32_t size = out.size();
  os.write(reinterpret_cast<const char *>(&size), sizeof(uint32_t));
  os.write(out.data(), size);
}

static void dumpBinaryDescriptor(pi_device_binary binary, pi_uint32 idx) {
  std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};
  auto path = outDir / (std::to_string(idx) + ".desc");

  std::ofstream os{path, std::ios::binary};

  const auto writeInt = [&os](const auto num) {
    os.write(reinterpret_cast<const char *>(&num), sizeof(num));
  };

  const auto writeString = [&os, &writeInt](const char *str) {
    size_t length = strlen(str) + 1;
    writeInt(length);
    os.write(str, length);
  };

  writeInt(binary->Version);
  writeInt(binary->Kind);
  writeInt(binary->Format);
  writeString(binary->DeviceTargetSpec);
  writeString(binary->CompileOptions);
  writeString(binary->LinkOptions);
  size_t numOffloadEntries =
      std::distance(binary->EntriesBegin, binary->EntriesEnd);
  writeInt(numOffloadEntries);

  for (auto it = binary->EntriesBegin; it != binary->EntriesEnd; ++it) {
    writeInt(reinterpret_cast<size_t>(it->addr));
    writeString(it->name);
    writeInt(static_cast<size_t>(it->size));
  }

  size_t numPropSets =
      std::distance(binary->PropertySetsBegin, binary->PropertySetsEnd);
  writeInt(numPropSets);
  for (auto it = binary->PropertySetsBegin; it != binary->PropertySetsEnd;
       ++it) {
    writeString(it->Name);
    size_t numProperties =
        std::distance(it->PropertiesBegin, it->PropertiesEnd);
    writeInt(numProperties);
    for (auto p = it->PropertiesBegin; p != it->PropertiesEnd; ++p) {
      writeString(p->Name);
      writeInt(reinterpret_cast<size_t>(p->ValAddr));
      writeInt(p->Type);
      writeInt(static_cast<size_t>(p->ValSize));
    }
  }
  os.close();
}

template <typename T, typename... Ts>
static void collectArgs(dpcpp_trace::APICall &call, T &&arg, Ts &&...rest) {
  auto &savedArg = *call.add_args();
  if constexpr (std::is_same_v<T, const char *>) {
    savedArg.set_type(dpcpp_trace::ArgData::STRING);
    savedArg.set_str_val(arg);
  } else if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
    uint64_t ptr = bit_cast<uint64_t>(arg);
    savedArg.set_type(dpcpp_trace::ArgData::POINTER);
    savedArg.set_int_val(ptr);
  } else if constexpr (std::is_unsigned_v<T>) {
    uint64_t val = static_cast<uint64_t>(arg);
    savedArg.set_int_val(val);

    if (sizeof(T) == 4)
      savedArg.set_type(dpcpp_trace::ArgData::UINT32);
    else if (sizeof(T) == 8)
      savedArg.set_type(dpcpp_trace::ArgData::UINT64);
  } else {
    int64_t val = static_cast<int64_t>(arg);
    savedArg.set_int_val(bit_cast<uint64_t>(val));

    if (sizeof(T) == 4)
      savedArg.set_type(dpcpp_trace::ArgData::INT32);
    else if (sizeof(T) == 8)
      savedArg.set_type(dpcpp_trace::ArgData::INT64);
  }

  if constexpr (sizeof...(Ts) > 0) {
    collectArgs(call, rest...);
  }
}

void handleSelectBinary(std::ostream &os, const uint32_t &funcId,
                        const uint64_t &begin, const uint64_t end,
                        const pi_plugin &, std::optional<pi_result> res,
                        pi_device device, pi_device_binary *binaries,
                        pi_uint32 numBinaries, pi_uint32 *selectedBinary) {
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
      dumpBinaryDescriptor(binaries[i], i);
    }
  }

  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, device, binaries, numBinaries, selectedBinary);
  call.add_small_outputs(reinterpret_cast<char *>(selectedBinary),
                         sizeof(selectedBinary));

  serialize(call, os);
}

void handlePlatformsGet(std::ostream &os, const uint32_t &funcId,
                        const uint64_t &begin, const uint64_t &end,
                        const pi_plugin &, std::optional<pi_result> res,
                        pi_uint32 numEntries, pi_platform *platforms,
                        pi_uint32 *numPlatforms) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, numEntries, platforms, numPlatforms);

  if (numPlatforms != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    call.add_small_outputs(reinterpret_cast<char *>(numPlatforms), outSize);
  }

  serialize(call, os);
}

void handleDevicesGet(std::ostream &os, const uint32_t &funcId,
                      const uint64_t &begin, const uint64_t &end,
                      const pi_plugin &, std::optional<pi_result> res,
                      pi_platform platform, pi_device_type type,
                      pi_uint32 numEntries, pi_device *devs,
                      pi_uint32 *numDevices) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, platform, type, numEntries, devs, numDevices);

  if (numDevices != nullptr) {
    size_t outSize = sizeof(pi_uint32);
    call.add_small_outputs(reinterpret_cast<char *>(numDevices), outSize);
  }

  serialize(call, os);
}

void handleEnqueueMemBufferMap(
    std::ostream &os, const uint64_t &eventId, const uint32_t &funcId,
    const uint64_t &begin, const uint64_t &end, bool writeMemObj,
    const pi_plugin &Plugin, std::optional<pi_result> res,
    pi_queue command_queue, pi_mem buffer, pi_bool blocking_map,
    pi_map_flags map_flags, size_t offset, size_t size,
    pi_uint32 num_events_in_wait_list, const pi_event *event_wait_list,
    pi_event *event, void **ret_map) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, command_queue, buffer, blocking_map, map_flags, offset,
              size, num_events_in_wait_list, event_wait_list, event, ret_map);

  if (writeMemObj) {
    std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};
    std::array<char, 1024> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};

    filename += "_" + std::to_string(eventId) + ".mem";

    call.add_mem_obj_outputs(filename);

    std::ofstream memOS{outDir / kBuffersPath / filename};

    // Wait for map to finish. There's no need to wait, if user asked to skip
    // mem objects. This will provide more accurate performance statistics.
    Plugin.PiFunctionTable.piEventsWait(1, event);

    memOS.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    memOS.write(static_cast<const char *>(*ret_map), size);
    memOS.close();
  }

  serialize(call, os);
}

void handleEnqueueMemBufferRead(
    std::ostream &os, bool writeMemObj, const uint64_t &eventId,
    const uint32_t &funcId, const uint64_t &begin, const uint64_t &end,
    const pi_plugin &Plugin, std::optional<pi_result> res, pi_queue queue,
    pi_mem buffer, pi_bool blocking_read, size_t offset, size_t size, void *ptr,
    pi_uint32 num_events_in_wait_list, const pi_event *event_wait_list,
    pi_event *event) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, queue, buffer, blocking_read, offset, size, ptr,
              num_events_in_wait_list, event_wait_list, event);

  if (writeMemObj) {
    std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};
    std::array<char, 1024> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};

    filename += "_" + std::to_string(eventId) + ".mem";

    call.add_mem_obj_outputs(filename);

    std::ofstream memOS{outDir / kBuffersPath / filename};

    // Wait for map to finish. There's no need to wait, if user asked to skip
    // mem objects. This will provide more accurate performance statistics.
    Plugin.PiFunctionTable.piEventsWait(1, event);

    memOS.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    memOS.write(static_cast<const char *>(ptr), size);
    memOS.close();
  }

  serialize(call, os);
}

void handleKernelGetGroupInfo(std::ostream &os, const uint32_t &funcId,
                              const uint64_t &begin, const uint64_t &end,
                              const pi_plugin &, std::optional<pi_result> res,
                              pi_kernel kernel, pi_device device,
                              pi_kernel_group_info param_name, size_t Size,
                              void *Value, size_t *RetSize) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, kernel, device, param_name, Size, Value, RetSize);

  if (Value != nullptr) {
    call.add_small_outputs(reinterpret_cast<char *>(Value), Size);
  }
  if (RetSize != nullptr) {
    size_t outSize = sizeof(size_t);
    call.add_small_outputs(reinterpret_cast<char *>(RetSize), outSize);
  }

  serialize(call, os);
}

template <typename T1, typename T2>
void handleGetInfo(std::ostream &os, const uint32_t &funcId,
                   const uint64_t &begin, const uint64_t &end,
                   const pi_plugin &, std::optional<pi_result> res, T1 obj,
                   T2 param_name, size_t Size, void *Value, size_t *RetSize) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, obj, param_name, Size, Value, RetSize);

  if (Value != nullptr) {
    call.add_small_outputs(reinterpret_cast<char *>(Value), Size);
  }
  if (RetSize != nullptr) {
    size_t outSize = sizeof(size_t);
    call.add_small_outputs(reinterpret_cast<char *>(RetSize), outSize);
  }

  serialize(call, os);
}

void handleUSMEnqueueMemcpy(std::ostream &os, bool writeMemObj,
                            const uint64_t &eventId, const uint32_t &funcId,
                            const uint64_t &begin, const uint64_t &end,
                            const pi_plugin &pluginInfo,
                            std::optional<pi_result> res, pi_queue queue,
                            pi_bool blocking, void *dst_ptr,
                            const void *src_ptr, size_t size,
                            pi_uint32 num_events_in_waitlist,
                            const pi_event *events_waitlist, pi_event *event) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  call.set_return_value(res.value());
  collectArgs(call, queue, blocking, dst_ptr, src_ptr, size,
              num_events_in_waitlist, events_waitlist, event);

  pi_context context;
  pluginInfo.PiFunctionTable.piQueueGetInfo(
      queue, PI_QUEUE_INFO_CONTEXT, sizeof(pi_context), &context, nullptr);
  pi_usm_type allocType;
  pluginInfo.PiFunctionTable.piextUSMGetMemAllocInfo(
      context, dst_ptr, PI_MEM_ALLOC_TYPE, sizeof(allocType), &allocType,
      nullptr);

  // if type is unknown, assuming unregistered host pointer.
  const bool shouldSaveMem =
      (allocType == PI_MEM_TYPE_UNKNOWN || allocType == PI_MEM_TYPE_HOST) &&
      writeMemObj;

  if (shouldSaveMem) {
    std::filesystem::path outDir{std::getenv(kTracePathEnvVar)};
    std::array<char, 1024> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};

    filename += "_" + std::to_string(eventId) + ".mem";

    call.add_mem_obj_outputs(filename);

    std::ofstream memOS{outDir / kBuffersPath / filename};

    // Wait for map to finish. There's no need to wait, if user asked to skip
    // mem objects. This will provide more accurate performance statistics.
    pluginInfo.PiFunctionTable.piEventsWait(1, event);

    memOS.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    memOS.write(static_cast<const char *>(dst_ptr), size);
    memOS.close();
  }

  serialize(call, os);
}

template <typename... Ts>
static void basicHandler(std::ostream &os, const uint32_t &funcId,
                         const uint64_t &begin, const uint64_t &end,
                         const pi_plugin &, std::optional<pi_result> res,
                         Ts... args) {
  dpcpp_trace::APICall call;
  call.set_function_id(funcId);
  call.set_time_start(begin);
  call.set_time_end(end);
  collectArgs(call, args...);
  call.set_return_value(res.value());
  serialize(call, os);
}

RecordHandler::RecordHandler(
    std::unique_ptr<std::ostream> os,
    std::chrono::time_point<std::chrono::steady_clock> timestamp,
    bool skipMemObjects)
    : mOS(std::move(os)), mStartTime(timestamp),
      mSkipMemObjects(skipMemObjects) {

#define _PI_API(api)                                                           \
  mArgHandler.set##_##api([this](auto &&...Args) {                             \
    basicHandler(*mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,        \
                 Args...);                                                     \
  });
#include <CL/sycl/detail/pi.def>
#undef _PI_API

  const auto wrap = [this](auto func) {
    return [this, func](auto &&...args) {
      std::invoke(func, *mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,
                  args...);
    };
  };

  const auto wrapMem = [this](auto func) {
    return [this, func](auto &&...args) {
      std::invoke(func, *mOS, mLastEventId, mLastFunctionId, mTimestampBegin,
                  mTimestampEnd, mSkipMemObjects, args...);
    };
  };

  mArgHandler.set_piextDeviceSelectBinary(wrap(handleSelectBinary));
  mArgHandler.set_piPlatformsGet(wrap(handlePlatformsGet));
  mArgHandler.set_piDevicesGet(wrap(handleDevicesGet));
  mArgHandler.set_piEnqueueMemBufferMap(wrapMem(handleEnqueueMemBufferMap));
  mArgHandler.set_piEnqueueMemBufferRead(wrapMem(handleEnqueueMemBufferRead));
  mArgHandler.set_piextUSMEnqueueMemcpy(wrapMem(handleUSMEnqueueMemcpy));
  mArgHandler.set_piPlatformGetInfo([this](auto &&...Args) {
    handleGetInfo(*mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,
                  Args...);
  });
  mArgHandler.set_piDeviceGetInfo([this](auto &&...Args) {
    handleGetInfo(*mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,
                  Args...);
  });
  mArgHandler.set_piContextGetInfo([this](auto &&...Args) {
    handleGetInfo(*mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,
                  Args...);
  });
  mArgHandler.set_piKernelGetInfo([this](auto &&...Args) {
    handleGetInfo(*mOS, mLastFunctionId, mTimestampBegin, mTimestampEnd,
                  Args...);
  });
  mArgHandler.set_piKernelGetGroupInfo(wrap(handleKernelGetGroupInfo));
}

void RecordHandler::handle(uint64_t eventId, uint32_t funcId,
                           const pi_plugin &plugin,
                           std::optional<pi_result> result, void *data) {
  mLastEventId = eventId;
  mLastFunctionId = funcId;
  mArgHandler.handle(funcId, plugin, result, data);
}

void RecordHandler::flush() { mOS->flush(); }

void RecordHandler::timestamp_begin() {
  const auto end = std::chrono::steady_clock::now();
  mTimestampBegin =
      std::chrono::duration_cast<std::chrono::microseconds>(end - mStartTime)
          .count();
}

void RecordHandler::timestamp_end() {
  const auto end = std::chrono::steady_clock::now();
  mTimestampEnd =
      std::chrono::duration_cast<std::chrono::microseconds>(end - mStartTime)
          .count();
}
