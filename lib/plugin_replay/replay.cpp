#include <CL/sycl/detail/pi.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <fstream>

using namespace sycl::detail;

size_t GOffset = 0;
std::ifstream GTrace;

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

template <typename T1, typename T2>
static void replayGetInfo(T1, T2, size_t size, void *value, size_t *retSize) {
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

  replayGetInfo(platform, param_name, param_value_size, param_value,
                param_value_size_ret);

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

  replayGetInfo(device, param_name, param_value_size, param_value,
                param_value_size_ret);

  return getResult();
}

pi_result piDeviceRetain(pi_device) { return skipRecord(); }

pi_result piDeviceRelease(pi_device) { return skipRecord(); }

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
  _PI_CL(piTearDown);

  return PI_SUCCESS;
}
}
