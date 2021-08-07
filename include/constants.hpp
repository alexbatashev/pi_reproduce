#pragma once

inline constexpr auto kSkipMemObjsEnvVar = "DPCPP_TRACE_SKIP_MEM_OBJECTS";
inline constexpr auto kTracePathEnvVar = "DPCPP_TRACE_DATA_PATH";
inline constexpr auto kPIDebugStreamName = "sycl.pi.debug";

inline constexpr auto kFilesConfigName = "files_config.json";
inline constexpr auto kPackedDataPath = "pack";

inline constexpr auto kBuffersPath = "buffers";

inline constexpr auto kPiTraceExt = ".pi_trace";

// Replay config constants
inline constexpr auto kReplayConfigName = "replay_config.json";
inline constexpr auto kReplayFileMapConfigName = "replay_file_map.json";

inline constexpr auto kReplayCommand = "command";
inline constexpr auto kReplayExecutable = "executable";
inline constexpr auto kReplayArguments = "arguments";

inline constexpr auto kRecordMode = "recordMode";
inline constexpr auto kRecordModeTraceOnly = "traceOnly";
inline constexpr auto kRecordModeDefault = "default";
inline constexpr auto kRecordModeFull = "full";

inline constexpr auto kHasOpenCLPlugin = "hasOpenCLPlugin";
inline constexpr auto kHasLevelZeroPlugin = "hasLevelZeroPlugin";
inline constexpr auto kHasCUDAPlugin = "hasCUDAPlugin";
inline constexpr auto kHasROCmPlugin = "hasROCmPlugin";

inline constexpr auto kOpenCLPluginName = "libpi_opencl.so";
inline constexpr auto kLevelZeroPluginName = "libpi_level_zero.so";
inline constexpr auto kCUDAPluginName = "libpi_cuda.so";
inline constexpr auto kROCmPluginName = "libpi_rocm.so";
