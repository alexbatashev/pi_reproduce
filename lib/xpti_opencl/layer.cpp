#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_0_APIS
#include <CL/cl.h>
#include <CL/cl_icd.h>
#include <CL/cl_layer.h>

#include <cstdint>
#include <iostream>

#include <utils/traits.hpp>
#include <xpti_trace_framework.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#define str(s) #s
#define xstr(s) str(s)

using namespace dpcpp_trace;

inline constexpr const char *OPENCL_STREAM_NAME = "opencl.debug";
xpti_td *GAPICallEvent = nullptr;

static struct _cl_icd_dispatch dispatch;
static const struct _cl_icd_dispatch *tdispatch;

static void initDispatch();

CL_API_ENTRY cl_int CL_API_CALL clGetLayerInfo(cl_layer_info paramName,
                                               size_t paramValueSize,
                                               void *paramValue,
                                               size_t *paramValueSizeRet) {
  switch (paramName) {
  case CL_LAYER_API_VERSION:
    if (paramValue) {
      if (paramValueSize < sizeof(cl_layer_api_version)) {
        return CL_INVALID_VALUE;
      }
      *static_cast<cl_layer_api_version *>(paramValue) =
          CL_LAYER_API_VERSION_100;
    }
    if (paramValueSizeRet) {
      *paramValueSizeRet = sizeof(cl_layer_api_version);
    }
    break;
  default:
    return CL_INVALID_VALUE;
  }

  return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clInitLayer(
    cl_uint numEntries, const struct _cl_icd_dispatch *targetDispatch,
    cl_uint *numEntriesOut, const struct _cl_icd_dispatch **layerDispatchRet) {
  if (!targetDispatch || !numEntriesOut || !layerDispatchRet)
    return CL_INVALID_VALUE;
  if (numEntries < sizeof(dispatch) / sizeof(dispatch.clGetPlatformIDs))
    return CL_INVALID_VALUE;

  xptiFrameworkInitialize();
  if (xptiTraceEnabled()) {
    uint8_t streamID = xptiRegisterStream(OPENCL_STREAM_NAME);
    xptiInitialize(OPENCL_STREAM_NAME, 0, 1, "0.1");

    xpti::payload_t OCLArgPayload(
        "OpenCL XPTI Layer (with function arguments)");
    uint64_t InstanceNo;
    GAPICallEvent = xptiMakeEvent("OpenCL XPTI Layer with arguments",
                                  &OCLArgPayload, xpti::trace_algorithm_event,
                                  xpti_at::active, &InstanceNo);
  }

  tdispatch = targetDispatch;

  initDispatch();

  *layerDispatchRet = &dispatch;
  *numEntriesOut = sizeof(dispatch) / sizeof(dispatch.clGetPlatformIDs);

  return CL_SUCCESS;
}

static uint64_t emitFunctionBeginEvent(uint32_t functionId,
                                       const char *funcName,
                                       unsigned char *argsData) {
  uint8_t streamID = xptiRegisterStream(OPENCL_STREAM_NAME);
  uint64_t correlationID = xptiGetUniqueId();

  xpti::function_with_args_t Payload{functionId, funcName, argsData, nullptr,
                                     nullptr};

  xptiNotifySubscribers(
      streamID, (uint16_t)xpti::trace_point_type_t::function_with_args_begin,
      GAPICallEvent, nullptr, correlationID, &Payload);

  return correlationID;
}

static void emitFunctionEndEvent(uint64_t correlationID, uint32_t functionId,
                                 const char *funcName, unsigned char *argsData,
                                 unsigned char *retValue) {
  uint8_t streamID = xptiRegisterStream(OPENCL_STREAM_NAME);

  xpti::function_with_args_t Payload{functionId, funcName, argsData, retValue,
                                     nullptr};

  xptiNotifySubscribers(
      streamID, (uint16_t)xpti::trace_point_type_t::function_with_args_begin,
      GAPICallEvent, nullptr, correlationID, &Payload);
}

enum class OCLApiKind : uint32_t {
#define _API_OCL(api) api,
#include "xpti_opencl/api.def"
#undef _API_OCL
};

template <OCLApiKind Kind, size_t Idx, typename... Args>
struct array_fill_helper;

template <OCLApiKind Kind> struct OCLApiArgTuple;

#define _API_OCL(api)                                                          \
  template <> struct OCLApiArgTuple<OCLApiKind::api> {                         \
    using type = typename function_traits<                                     \
        std::remove_pointer_t<decltype(dispatch.api)>>::args_type;             \
  };

#include "xpti_opencl/api.def"
#undef _API_OCL

template <OCLApiKind Kind, size_t Idx, typename T>
struct array_fill_helper<Kind, Idx, T> {
  static void fill(unsigned char *dst, T &&arg) {
    using ArgsTuple = typename OCLApiArgTuple<Kind>::type;
    // C-style cast is required here.
    auto realArg = (std::tuple_element_t<Idx, ArgsTuple>)(arg);
    *(std::remove_cv_t<std::tuple_element_t<Idx, ArgsTuple>> *)dst = realArg;
  }
};

template <OCLApiKind Kind, size_t Idx, typename T, typename... Args>
struct array_fill_helper<Kind, Idx, T, Args...> {
  static void fill(unsigned char *dst, const T &&arg, Args &&...rest) {
    using ArgsTuple = typename OCLApiArgTuple<Kind>::type;
    // C-style cast is required here.
    auto realArg = (std::tuple_element_t<Idx, ArgsTuple>)(arg);
    *(std::remove_cv_t<std::tuple_element_t<Idx, ArgsTuple>> *)dst = realArg;
    array_fill_helper<Kind, Idx + 1, Args...>::fill(
        dst + sizeof(decltype(realArg)), std::forward<Args>(rest)...);
  }
};

template <typename... Ts>
constexpr size_t totalSize(const std::tuple<Ts...> &) {
  if constexpr (sizeof...(Ts) == 0)
    return 0;
  else
    return (sizeof(Ts) + ...);
}

template <OCLApiKind Kind, typename... ArgsT>
auto packCallArguments(ArgsT &&...args) {
  using ArgsTuple = typename OCLApiArgTuple<Kind>::type;

  constexpr size_t totalSizeVal = totalSize(ArgsTuple{});

  std::array<unsigned char, totalSizeVal> argsData;
  array_fill_helper<Kind, 0, ArgsT...>::fill(argsData.data(),
                                             std::forward<ArgsT>(args)...);

  return argsData;
}

#define _API_OCL(api)                                                          \
  static const auto wrap_##api = [](auto... args) {                            \
    if (xptiTraceEnabled()) {                                                  \
    }                                                                          \
    auto argsData = packCallArguments<OCLApiKind::api>(                        \
        std::forward<decltype(args)>(args)...);                                \
    uint32_t funcID = static_cast<uint32_t>(OCLApiKind::api);                  \
    uint64_t corrID =                                                          \
        emitFunctionBeginEvent(funcID, xstr(api), argsData.data());            \
    if constexpr (std::is_same_v<                                              \
                      void, typename function_traits<std::remove_pointer_t<    \
                                decltype(dispatch.api)>>::ret_type>) {         \
      tdispatch->api(args...);                                                 \
      emitFunctionEndEvent(corrID, funcID, xstr(api), argsData.data(),         \
                           nullptr);                                           \
    } else {                                                                   \
      auto result = tdispatch->api(args...);                                   \
      emitFunctionEndEvent(corrID, funcID, xstr(api), argsData.data(),         \
                           reinterpret_cast<unsigned char *>(&result));        \
      return result;                                                           \
    }                                                                          \
  };
#include "xpti_opencl/api.def"
#undef _API_OCL

static void initDispatch() {
#define _API_OCL(api) dispatch.api = wrap_##api;

#include "xpti_opencl/api.def"
#undef _API_OCL
}
