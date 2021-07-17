#include "constants.hpp"
#include "trace_reader.hpp"

#include <CL/sycl/detail/pi.hpp>

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <pthread.h>

using namespace sycl::detail;

size_t GOffset = 0;
thread_local std::ifstream GTrace;

std::map<pi_kernel, pi_program> GKernelProgramMap;

static void ensureTraceOpened() {
  if (!GTrace.is_open()) {
    std::filesystem::path traceDir{getenv(kTracePathEnvVar)};
    std::array<char, 1024> buf;
    pthread_getname_np(pthread_self(), buf.data(), buf.size());
    std::string filename{buf.data()};
    filename += ".trace";
    auto traceFile = traceDir / filename;

    GTrace = std::ifstream(traceFile, std::ios::binary);
  }
}

void dieIfUnexpected(uint32_t funcId, PiApiKind expected) {
  if (funcId != static_cast<uint32_t>(expected)) {
    std::cerr << "Unexpected PI call\n";
    std::terminate();
  }
}

void replayGetInfo(Record &record, void *paramValue, size_t *retSize) {
  char *ptr = paramValue ? static_cast<char *>(paramValue)
                         : reinterpret_cast<char *>(retSize);
  std::uninitialized_copy(record.outputs[0].data.begin(),
                          record.outputs[0].data.end(), ptr);
}

extern "C" {
pi_result piPlatformsGet(pi_uint32 numEntries, pi_platform *platforms,
                         pi_uint32 *numPlatforms) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piPlatformsGet);

  if (numPlatforms != nullptr) {
    *numPlatforms =
        *reinterpret_cast<pi_uint32 *>(record.outputs[0].data.data());
  }

  if (numEntries > 0 && platforms != nullptr) {
    for (size_t i = 0; i < numEntries; i++) {
      platforms[i] = reinterpret_cast<pi_platform>(new int{1});
    }
  }

  return record.result;
}

pi_result piPlatformGetInfo(pi_platform platform, pi_platform_info param_name,
                            size_t param_value_size, void *param_value,
                            size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piPlatformGetInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  return record.result;
}

pi_result piDevicesGet(pi_platform platform, pi_device_type type,
                       pi_uint32 numEntries, pi_device *devs,
                       pi_uint32 *numDevices) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piDevicesGet);

  if (numDevices != nullptr) {
    *numDevices = *reinterpret_cast<pi_uint32 *>(record.outputs[0].data.data());
  }

  if (numEntries > 0 && devs != nullptr) {
    for (size_t i = 0; i < numEntries; i++) {
      devs[i] = reinterpret_cast<pi_device>(new int{1});
    }
  }

  return record.result;
}

pi_result piDeviceGetInfo(pi_device device, pi_device_info param_name,
                          size_t param_value_size, void *param_value,
                          size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piDeviceGetInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  return record.result;
}

pi_result piDeviceRetain(pi_device) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piDeviceRetain);
  return record.result;
}

pi_result piDeviceRelease(pi_device) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piDeviceRelease);
  return record.result;
}

pi_result piContextCreate(const pi_context_properties *properties,
                          pi_uint32 num_devices, const pi_device *devices,
                          void (*pfn_notify)(const char *errinfo,
                                             const void *private_info,
                                             size_t cb, void *user_data),
                          void *user_data, pi_context *ret_context) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piContextCreate);
  *ret_context = reinterpret_cast<pi_context>(new int{1});
  return record.result;
}

pi_result piContextGetInfo(pi_context context, pi_context_info param_name,
                           size_t param_value_size, void *param_value,
                           size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piContextGetInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  return record.result;
}

pi_result piContextRelease(pi_context) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piContextRelease);
  return record.result;
}

pi_result piContextRetain(pi_context) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piContextRetain);
  return record.result;
}

pi_result piQueueCreate(pi_context context, pi_device device,
                        pi_queue_properties properties, pi_queue *queue) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piQueueCreate);
  *queue = reinterpret_cast<pi_queue>(new int{1});
  return record.result;
}

pi_result piQueueGetInfo(pi_queue command_queue, pi_queue_info param_name,
                         size_t param_value_size, void *param_value,
                         size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piQueueGetInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  return record.result;
}

pi_result piQueueRetain(pi_queue) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piQueueRetain);
  return record.result;
}

pi_result piQueueRelease(pi_queue command_queue) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piQueueRelease);
  return record.result;
}

pi_result piQueueFinish(pi_queue command_queue) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piQueueFinish);
  return record.result;
}

pi_result piMemBufferCreate(pi_context context, pi_mem_flags flags, size_t size,
                            void *host_ptr, pi_mem *ret_mem,
                            const pi_mem_properties *properties) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piMemBufferCreate);
  *ret_mem = reinterpret_cast<pi_mem>(new int{1});
  return record.result;
}

pi_result piextDeviceSelectBinary(pi_device device, pi_device_binary *binaries,
                                  pi_uint32 num_binaries,
                                  pi_uint32 *selected_binary_ind) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextDeviceSelectBinary);
  *selected_binary_ind =
      *reinterpret_cast<pi_uint32 *>(record.outputs[0].data.data());

  return record.result;
}

pi_result piProgramCreateWithBinary(
    pi_context context, pi_uint32 num_devices, const pi_device *device_list,
    const size_t *lengths, const unsigned char **binaries,
    size_t num_metadata_entries, const pi_device_binary_property *metadata,
    pi_int32 *binary_status, pi_program *ret_program) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piProgramCreateWithBinary);
  *ret_program = reinterpret_cast<pi_program>(new int{1});
  return record.result;
}

pi_result piProgramBuild(pi_program program, pi_uint32 num_devices,
                         const pi_device *device_list, const char *options,
                         void (*pfn_notify)(pi_program program,
                                            void *user_data),
                         void *user_data) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piProgramBuild);
  return record.result;
}

pi_result piKernelCreate(pi_program program, const char *kernel_name,
                         pi_kernel *ret_kernel) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelCreate);
  *ret_kernel = reinterpret_cast<pi_kernel>(new int{1});
  GKernelProgramMap[*ret_kernel] = program;
  return record.result;
}

pi_result piKernelSetExecInfo(pi_kernel kernel, pi_kernel_exec_info value_name,
                              size_t param_value_size,
                              const void *param_value) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelSetExecInfo);
  return record.result;
}

pi_result piKernelGetInfo(pi_kernel kernel, pi_kernel_info param_name,
                          size_t param_value_size, void *param_value,
                          size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelGetInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  if (param_name == PI_KERNEL_INFO_PROGRAM && param_value) {
    *static_cast<pi_program *>(param_value) = GKernelProgramMap[kernel];
  }

  return record.result;
}
pi_result piKernelGetGroupInfo(pi_kernel kernel, pi_device device,
                               pi_kernel_group_info param_name,
                               size_t param_value_size, void *param_value,
                               size_t *param_value_size_ret) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelGetGroupInfo);

  replayGetInfo(record, param_value, param_value_size_ret);

  return record.result;
}

pi_result piextKernelSetArgMemObj(pi_kernel kernel, pi_uint32 arg_index,
                                  const pi_mem *arg_value) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextKernelSetArgMemObj);
  return record.result;
}

pi_result piKernelSetArg(pi_kernel kernel, pi_uint32 arg_index, size_t arg_size,
                         const void *arg_value) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelSetArg);
  return record.result;
}

pi_result piEnqueueKernelLaunch(
    pi_queue queue, pi_kernel kernel, pi_uint32 work_dim,
    const size_t *global_work_offset, const size_t *global_work_size,
    const size_t *local_work_size, pi_uint32 num_events_in_wait_list,
    const pi_event *event_wait_list, pi_event *event) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEnqueueKernelLaunch);
  *event = reinterpret_cast<pi_event>(new int{1});
  return record.result;
}

pi_result piEnqueueMemUnmap(pi_queue command_queue, pi_mem memobj,
                            void *mapped_ptr, pi_uint32 num_events_in_wait_list,
                            const pi_event *event_wait_list, pi_event *event) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEnqueueMemUnmap);
  *event = reinterpret_cast<pi_event>(new int{1});
  delete[] static_cast<char *>(mapped_ptr);
  return record.result;
}

pi_result piEventsWait(pi_uint32 num_events, const pi_event *event_list) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEventsWait);
  return record.result;
}

pi_result piEventRelease(pi_event) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEventRelease);
  return record.result;
}

pi_result piMemRelease(pi_mem) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piMemRelease);
  return record.result;
}

pi_result piProgramRelease(pi_program) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piProgramRelease);
  return record.result;
}

pi_result piKernelRelease(pi_kernel) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piKernelRelease);
  return record.result;
}

pi_result piEnqueueMemBufferMap(pi_queue command_queue, pi_mem buffer,
                                pi_bool blocking_map, pi_map_flags map_flags,
                                size_t offset, size_t size,
                                pi_uint32 num_events_in_wait_list,
                                const pi_event *event_wait_list,
                                pi_event *event, void **ret_map) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEnqueueMemBufferMap);

  auto memory = new char[record.outputs[0].data.size()];

  std::memcpy(memory, record.outputs[0].data.data(),
              record.outputs[0].data.size());

  *ret_map = memory;
  *event = reinterpret_cast<pi_event>(new int{1});

  return record.result;
}

pi_result piEnqueueMemBufferRead(pi_queue queue, pi_mem buffer,
                                 pi_bool blocking_read, size_t offset,
                                 size_t size, void *ptr,
                                 pi_uint32 num_events_in_wait_list,
                                 const pi_event *event_wait_list,
                                 pi_event *event) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piEnqueueMemBufferRead);

  std::memcpy(ptr, record.outputs[0].data.data(),
              record.outputs[0].data.size());
  *event = reinterpret_cast<pi_event>(new int{1});

  return record.result;
}

pi_result piextUSMEnqueueMemcpy(pi_queue queue, pi_bool blocking, void *dst_ptr,
                                const void *src_ptr, size_t size,
                                pi_uint32 num_events_in_waitlist,
                                const pi_event *events_waitlist,
                                pi_event *event) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextUSMEnqueueMemcpy);

  if (record.outputs.size() > 0) {
    std::memcpy(dst_ptr, record.outputs[0].data.data(), size);
  }

  *event = reinterpret_cast<pi_event>(new int{1});

  return record.result;
}

pi_result piextUSMHostAlloc(void **result_ptr, pi_context context,
                            pi_usm_mem_properties *properties, size_t size,
                            pi_uint32 alignment) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextUSMHostAlloc);
  *result_ptr = static_cast<void *>(new char[size]);

  return record.result;
}

pi_result piextUSMDeviceAlloc(void **result_ptr, pi_context context,
                              pi_device device,
                              pi_usm_mem_properties *properties, size_t size,
                              pi_uint32 alignment) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextUSMDeviceAlloc);
  *result_ptr = static_cast<void *>(new char[size]);

  return record.result;
}

pi_result piextUSMFree(pi_context context, void *ptr) {
  ensureTraceOpened();
  Record record = getNextRecord(GTrace, "", true);

  dieIfUnexpected(record.functionId, PiApiKind::piextUSMFree);

  delete[] static_cast<char *>(ptr);

  return record.result;
}

pi_result piTearDown(void *) {
  return PI_SUCCESS;
}

pi_result piPluginInit(pi_plugin *PluginInit) {

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

  _PI_CL(piextUSMEnqueueMemcpy);
  _PI_CL(piextUSMDeviceAlloc);
  _PI_CL(piextUSMHostAlloc);
  _PI_CL(piextUSMFree);

  _PI_CL(piTearDown);

  return PI_SUCCESS;
}
}
