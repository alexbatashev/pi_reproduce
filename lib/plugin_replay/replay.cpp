#include <CL/sycl/detail/pi.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>

using namespace sycl::detail;

size_t GOffset = 0;
std::ifstream GTrace;

std::map<pi_kernel, pi_program> GKernelProgramMap;

void seekfile() {}

std::tuple<pi_uint32, size_t, size_t> readHeader() {
  uint32_t functionId = 0;
  size_t numOutputs = 0;
  size_t totalSize = 0;

  GTrace.read(reinterpret_cast<char *>(&functionId), sizeof(uint32_t));
  GTrace.read(reinterpret_cast<char *>(&numOutputs), sizeof(size_t));
  GTrace.read(reinterpret_cast<char *>(&totalSize), sizeof(size_t));

  return {functionId, numOutputs, totalSize};
}
void dieIfUnexpected(uint32_t funcId, PiApiKind expected) {
  if (funcId != static_cast<uint32_t>(expected)) {
    std::cerr << "Unexpected PI call\n";
    std::terminate();
  }
}

static void skipArguments(size_t argSize) {
  std::array<char, 8192> buf;
  GTrace.read(buf.data(), argSize);
}

static void replayGetInfo(void *value, size_t *retSize) {
  char *outPtr = value == nullptr ? reinterpret_cast<char *>(retSize)
                                  : reinterpret_cast<char *>(value);
  size_t outSize;
  GTrace.read(reinterpret_cast<char *>(&outSize), sizeof(size_t));
  GTrace.read(reinterpret_cast<char *>(outPtr), outSize);
}

static pi_result getResult() {
  pi_result res;
  GTrace.read(reinterpret_cast<char *>(&res), sizeof(pi_result));

  return res;
}

static pi_result skipRecord() {
  auto [funcId, numOutputs, totalSize] = readHeader();

  skipArguments(totalSize);

  for (size_t i = 0; i < numOutputs; i++) {
    size_t dataSize;
    GTrace.read(reinterpret_cast<char *>(&dataSize), sizeof(size_t));
    GTrace.seekg(dataSize, std::ios_base::cur); // skip output
  }

  return getResult();
}

extern "C" {
pi_result piPlatformsGet(pi_uint32 numEntries, pi_platform *platforms,
                         pi_uint32 *numPlatforms) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piPlatformsGet);

  skipArguments(totalSize);

  if (numPlatforms != nullptr) {
    size_t size;
    GTrace.read(reinterpret_cast<char *>(&size), sizeof(size_t));
    GTrace.read(reinterpret_cast<char *>(numPlatforms), size);
  }

  if (numEntries > 0 && platforms != nullptr) {
    for (size_t i = 0; i < numEntries; i++) {
      platforms[i] = reinterpret_cast<pi_platform>(new int{1});
    }
  }

  return getResult();
}

pi_result piPlatformGetInfo(pi_platform platform, pi_platform_info param_name,
                            size_t param_value_size, void *param_value,
                            size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piPlatformGetInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  return getResult();
}

pi_result piDevicesGet(pi_platform platform, pi_device_type type,
                       pi_uint32 numEntries, pi_device *devs,
                       pi_uint32 *numDevices) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piDevicesGet);

  skipArguments(totalSize);

  if (numDevices != nullptr) {
    size_t size;
    GTrace.read(reinterpret_cast<char *>(&size), sizeof(size_t));
    GTrace.read(reinterpret_cast<char *>(numDevices), size);
  }

  if (numEntries > 0 && devs != nullptr) {
    for (size_t i = 0; i < numEntries; i++) {
      devs[i] = reinterpret_cast<pi_device>(new int{1});
    }
  }

  return getResult();
}

pi_result piDeviceGetInfo(pi_device device, pi_device_info param_name,
                          size_t param_value_size, void *param_value,
                          size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piDeviceGetInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  return getResult();
}

pi_result piDeviceRetain(pi_device) { return skipRecord(); }

pi_result piDeviceRelease(pi_device) { return skipRecord(); }

pi_result piContextCreate(const pi_context_properties *properties,
                          pi_uint32 num_devices, const pi_device *devices,
                          void (*pfn_notify)(const char *errinfo,
                                             const void *private_info,
                                             size_t cb, void *user_data),
                          void *user_data, pi_context *ret_context) {
  *ret_context = reinterpret_cast<pi_context>(new int{1});
  return skipRecord();
}

pi_result piContextGetInfo(pi_context context, pi_context_info param_name,
                           size_t param_value_size, void *param_value,
                           size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piContextGetInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  return getResult();
}

pi_result piContextRelease(pi_context) { return skipRecord(); }

pi_result piContextRetain(pi_context) { return skipRecord(); }

pi_result piQueueCreate(pi_context context, pi_device device,
                        pi_queue_properties properties, pi_queue *queue) {
  *queue = reinterpret_cast<pi_queue>(new int{1});
  return skipRecord();
}

pi_result piQueueGetInfo(pi_queue command_queue, pi_queue_info param_name,
                         size_t param_value_size, void *param_value,
                         size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piQueueGetInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  return getResult();
}

pi_result piQueueRetain(pi_queue) { return skipRecord(); }

pi_result piQueueRelease(pi_queue command_queue) { return skipRecord(); }

pi_result piQueueFinish(pi_queue command_queue) { return skipRecord(); }

pi_result piMemBufferCreate(pi_context context, pi_mem_flags flags, size_t size,
                            void *host_ptr, pi_mem *ret_mem,
                            const pi_mem_properties *properties) {
  *ret_mem = reinterpret_cast<pi_mem>(new int{1});
  return skipRecord();
}

pi_result piextDeviceSelectBinary(pi_device device, pi_device_binary *binaries,
                                  pi_uint32 num_binaries,
                                  pi_uint32 *selected_binary_ind) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piextDeviceSelectBinary);

  skipArguments(totalSize);

  size_t outSize = 0;
  GTrace.read(reinterpret_cast<char *>(&outSize), sizeof(size_t));
  GTrace.read(reinterpret_cast<char *>(selected_binary_ind), outSize);

  return getResult();
}

pi_result piProgramCreateWithBinary(pi_context context, pi_uint32 num_devices,
                                    const pi_device *device_list,
                                    const size_t *lengths,
                                    const unsigned char **binaries,
                                    pi_int32 *binary_status,
                                    pi_program *ret_program) {
  *ret_program = reinterpret_cast<pi_program>(new int{1});
  return skipRecord();
}

pi_result piProgramBuild(pi_program program, pi_uint32 num_devices,
                         const pi_device *device_list, const char *options,
                         void (*pfn_notify)(pi_program program,
                                            void *user_data),
                         void *user_data) {
  return skipRecord();
}

pi_result piKernelCreate(pi_program program, const char *kernel_name,
                         pi_kernel *ret_kernel) {
  *ret_kernel = reinterpret_cast<pi_kernel>(new int{1});
  GKernelProgramMap[*ret_kernel] = program;
  return skipRecord();
}

pi_result piKernelSetExecInfo(pi_kernel kernel, pi_kernel_exec_info value_name,
                              size_t param_value_size,
                              const void *param_value) {
  return skipRecord();
}

pi_result piKernelGetInfo(pi_kernel kernel, pi_kernel_info param_name,
                          size_t param_value_size, void *param_value,
                          size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piKernelGetInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  if (param_name == PI_KERNEL_INFO_PROGRAM && param_value) {
    *static_cast<pi_program *>(param_value) = GKernelProgramMap[kernel];
  }

  return getResult();
}
pi_result piKernelGetGroupInfo(pi_kernel kernel, pi_device device,
                               pi_kernel_group_info param_name,
                               size_t param_value_size, void *param_value,
                               size_t *param_value_size_ret) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piKernelGetGroupInfo);

  skipArguments(totalSize);

  replayGetInfo(param_value, param_value_size_ret);

  return getResult();
}

pi_result piextKernelSetArgMemObj(pi_kernel kernel, pi_uint32 arg_index,
                                  const pi_mem *arg_value) {
  return skipRecord();
}

pi_result piKernelSetArg(pi_kernel kernel, pi_uint32 arg_index, size_t arg_size,
                         const void *arg_value) {
  return skipRecord();
}

pi_result piEnqueueKernelLaunch(
    pi_queue queue, pi_kernel kernel, pi_uint32 work_dim,
    const size_t *global_work_offset, const size_t *global_work_size,
    const size_t *local_work_size, pi_uint32 num_events_in_wait_list,
    const pi_event *event_wait_list, pi_event *event) {
  *event = reinterpret_cast<pi_event>(new int{1});
  return skipRecord();
}

pi_result piEnqueueMemUnmap(pi_queue command_queue, pi_mem memobj,
                            void *mapped_ptr, pi_uint32 num_events_in_wait_list,
                            const pi_event *event_wait_list, pi_event *event) {
  *event = reinterpret_cast<pi_event>(new int{1});
  delete[] static_cast<char *>(mapped_ptr);
  return skipRecord();
}

pi_result piEventsWait(pi_uint32 num_events, const pi_event *event_list) {
  return skipRecord();
}

pi_result piEventRelease(pi_event) { return skipRecord(); }

pi_result piMemRelease(pi_mem) { return skipRecord(); }

pi_result piProgramRelease(pi_program) { return skipRecord(); }

pi_result piKernelRelease(pi_kernel) { return skipRecord(); }

pi_result piEnqueueMemBufferMap(pi_queue command_queue, pi_mem buffer,
                                pi_bool blocking_map, pi_map_flags map_flags,
                                size_t offset, size_t size,
                                pi_uint32 num_events_in_wait_list,
                                const pi_event *event_wait_list,
                                pi_event *event, void **ret_map) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piEnqueueMemBufferMap);

  skipArguments(totalSize);

  size_t outSize;
  GTrace.read(reinterpret_cast<char *>(&outSize), sizeof(size_t));

  auto memory = new char[outSize];

  GTrace.read(reinterpret_cast<char *>(memory), outSize);

  *ret_map = memory;

  return getResult();
}

pi_result piEnqueueMemBufferRead(pi_queue queue, pi_mem buffer,
                                 pi_bool blocking_read, size_t offset,
                                 size_t size, void *ptr,
                                 pi_uint32 num_events_in_wait_list,
                                 const pi_event *event_wait_list,
                                 pi_event *event) {
  auto [funcId, numOutputs, totalSize] = readHeader();

  dieIfUnexpected(funcId, PiApiKind::piEnqueueMemBufferRead);

  skipArguments(totalSize);

  size_t outSize;
  GTrace.read(reinterpret_cast<char *>(&outSize), sizeof(size_t));

  GTrace.read(reinterpret_cast<char *>(ptr), outSize);

  return getResult();
}

pi_result piTearDown(void *) { return PI_SUCCESS; }

pi_result piPluginInit(pi_plugin *PluginInit) {
  std::filesystem::path traceDir{getenv("PI_REPRODUCE_TRACE_PATH")};
  auto traceFile = traceDir / "trace.data";
  std::cerr << "Initialize PI replay plugin\n";
  std::cerr << "Trace file: " << traceFile << "\n";

  GTrace = std::ifstream(traceFile, std::ios::binary);

#define _PI_CL(pi_api)                                                         \
  (PluginInit->PiFunctionTable).pi_api = (decltype(&::pi_api))(&pi_api);

  _PI_CL(piPlatformsGet);
  _PI_CL(piPlatformGetInfo);

  _PI_CL(piDevicesGet);
  _PI_CL(piDeviceGetInfo);
  _PI_CL(piDeviceRetain);
  _PI_CL(piDeviceRelease);
  _PI_CL(piextDeviceSelectBinary);

  _PI_CL(piContextCreate);
  _PI_CL(piContextGetInfo);
  _PI_CL(piContextRelease);
  _PI_CL(piContextRetain);

  _PI_CL(piQueueCreate);
  _PI_CL(piQueueGetInfo);
  _PI_CL(piQueueRetain);
  _PI_CL(piQueueRelease);
  _PI_CL(piQueueFinish);

  _PI_CL(piMemBufferCreate);
  _PI_CL(piMemRelease);

  _PI_CL(piProgramBuild);
  _PI_CL(piProgramCreateWithBinary);
  _PI_CL(piProgramRelease);

  _PI_CL(piKernelCreate);
  _PI_CL(piKernelSetExecInfo);
  _PI_CL(piKernelGetInfo);
  _PI_CL(piKernelGetGroupInfo);
  _PI_CL(piextKernelSetArgMemObj);
  _PI_CL(piKernelSetArg);
  _PI_CL(piKernelRelease);

  _PI_CL(piEnqueueKernelLaunch);
  _PI_CL(piEnqueueMemBufferMap);
  _PI_CL(piEnqueueMemBufferRead);
  _PI_CL(piEnqueueMemUnmap);

  _PI_CL(piEventsWait);
  _PI_CL(piEventRelease);

  _PI_CL(piTearDown);

  return PI_SUCCESS;
}
}
