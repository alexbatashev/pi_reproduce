#pragma once

#include <CL/sycl/detail/pi.h>
#include <CL/sycl/detail/pi.hpp>
#include <fmt/core.h>
#include <iostream>

template <typename T> void printFormatted(T value) {
  fmt::print("{:>15} : {:#x}\n", "<unknown>", (size_t)(value));
}

template <> void printFormatted<pi_queue_info>(pi_queue_info info) {
  std::string infoName;
  switch (info) {
  case PI_QUEUE_INFO_CONTEXT:
    infoName = "PI_QUEUE_INFO_CONTEXT";
    break;
  case PI_QUEUE_INFO_DEVICE:
    infoName = "PI_QUEUE_INFO_DEVICE";
    break;
  case PI_QUEUE_INFO_DEVICE_DEFAULT:
    infoName = "PI_QUEUE_INFO_DEVICE_DEFAULT";
    break;
  case PI_QUEUE_INFO_PROPERTIES:
    infoName = "PI_QUEUE_INFO_PROPERTIES";
    break;
  case PI_QUEUE_INFO_REFERENCE_COUNT:
    infoName = "PI_QUEUE_INFO_REFERENCE_COUNT";
    break;
  case PI_QUEUE_INFO_SIZE:
    infoName = "PI_QUEUE_INFO_SIZE";
    break;
  default:
    infoName = "UNKNOWN";
  }
  fmt::print("{:>15} : {}\n", "<pi_queue_info>", infoName);
}

template <> void printFormatted<pi_platform_info>(pi_platform_info info) {
  std::string infoName;
  switch (info) {
  case PI_PLATFORM_INFO_EXTENSIONS:
    infoName = "PI_PLATFORM_INFO_EXTENSIONS";
    break;
  case PI_PLATFORM_INFO_NAME:
    infoName = "PI_PLATFORM_INFO_NAME";
    break;
  case PI_PLATFORM_INFO_PROFILE:
    infoName = "PI_PLATFORM_INFO_PROFILE";
    break;
  case PI_PLATFORM_INFO_VENDOR:
    infoName = "PI_PLATFORM_INFO_VENDOR";
    break;
  case PI_PLATFORM_INFO_VERSION:
    infoName = "PI_PLATFORM_INFO_VERSION";
    break;
  default:
    infoName = "UNKNOWN";
  }
  fmt::print("{:>15} : {}\n", "<pi_platform_info>", infoName);
}

static inline std::string join(std::vector<std::string> &values) {
  std::string formattedValues = "";
  for (auto it = values.begin(); it != values.end(); ++it) {
    formattedValues += *it;
    if (it != values.end() - 1)
      formattedValues += " | ";
  }

  return formattedValues;
}

void printQueueProperties(pi_queue_properties bitfield) {
  std::vector<std::string> values;

  if (bitfield == 0) {
    values.push_back("NONE");
  }

  if (bitfield & PI_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) {
    values.push_back("PI_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE");
  }
  if (bitfield & PI_QUEUE_PROFILING_ENABLE) {
    values.push_back("PI_QUEUE_PROFILING_ENABLE");
  }
  if (bitfield & PI_QUEUE_ON_DEVICE) {
    values.push_back("PI_QUEUE_ON_DEVICE");
  }
  if (bitfield & PI_QUEUE_ON_DEVICE_DEFAULT) {
    values.push_back("PI_QUEUE_ON_DEVICE_DEFAULT");
  }

  fmt::print("{:>15} : {:#x} ({})\n", "<pi_queue_properties>", bitfield,
             join(values));
}

void printMemFlags(pi_mem_flags bitfield) {
  std::vector<std::string> values;

  if (bitfield == 0) {
    values.push_back("NONE");
  }

  if (bitfield & PI_MEM_FLAGS_ACCESS_RW) {
    values.push_back("PI_MEM_FLAGS_ACCESS_RW");
  }
  if (bitfield & PI_MEM_FLAGS_HOST_PTR_USE) {
    values.push_back("PI_MEM_FLAGS_HOST_PTR_USE");
  }
  if (bitfield & PI_MEM_FLAGS_HOST_PTR_COPY) {
    values.push_back("PI_MEM_FLAGS_HOST_PTR_COPY");
  }
  if (bitfield & PI_MEM_FLAGS_HOST_PTR_ALLOC) {
    values.push_back("PI_MEM_FLAGS_HOST_PTR_ALLOC");
  }

  fmt::print("{:>15} : {:#x} ({})\n", "<pi_mem_flags>", bitfield, join(values));
}

void printMapFlags(pi_map_flags bitfield) {
  std::vector<std::string> values;

  if (bitfield == 0) {
    values.push_back("NONE");
  }

  if (bitfield & PI_MAP_READ) {
    values.push_back("PI_MAP_READ");
  }
  if (bitfield & PI_MAP_WRITE) {
    values.push_back("PI_MAP_WRITE");
  }
  if (bitfield & PI_MAP_WRITE_INVALIDATE_REGION) {
    values.push_back("PI_MAP_WRITE_INVALIDATE_REGION");
  }

  fmt::print("{:>15} : {:#x} ({})\n", "<pi_map_flags>", bitfield, join(values));
}

template <sycl::detail::PiApiKind Kind, typename T> struct PrintHelper {};

template <sycl::detail::PiApiKind Kind, typename T, typename... Ts>
struct PrintHelper<Kind, std::tuple<T, Ts...>> {
  static void print(T arg, Ts... args) {
    using PI = sycl::detail::PiApiKind;
    if constexpr (Kind == PI::piMemBufferCreate && sizeof...(Ts) == 4) {
      printMemFlags(arg);
    } else if constexpr (Kind == PI::piMemImageCreate && sizeof...(Ts) == 4) {
      printMemFlags(arg);
    } else if constexpr (Kind == PI::piMemBufferPartition &&
                         sizeof...(Ts) == 3) {
      printMemFlags(arg);
    } else if constexpr (Kind == PI::piQueueCreate && sizeof...(Ts) == 1) {
      printQueueProperties(arg);
    } else if constexpr (Kind == PI::piEnqueueMemBufferMap &&
                         sizeof...(Ts) == 6) {
      printQueueProperties(arg);
    } else {
      printFormatted<T>(arg);
    }
    if constexpr (sizeof...(Ts) > 0) {
      PrintHelper<Kind, std::tuple<Ts...>>::print(args...);
    }
  }
};
