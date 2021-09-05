#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_0_APIS

#include "Recorder.hpp"
#include "utils/traits.hpp"

#include <CL/cl_icd.h>

#include <functional>
#include <tuple>

namespace dpcpp_trace::detail {

template <typename TupleT, size_t... Is>
inline auto get(char *data, const std::index_sequence<Is...> &) {
  // Our type should be last in Is sequence
  using TargetType =
      typename std::tuple_element<sizeof...(Is) - 1, TupleT>::type;

  // Calculate sizeof all elements before target + target element then substract
  // sizeof target element
  const size_t offset =
      (sizeof(typename std::tuple_element<Is, TupleT>::type) + ...) -
      sizeof(TargetType);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  return *(typename std::decay<TargetType>::type *)(data + offset);
#pragma GCC diagnostic pop
}

template <typename TupleT, size_t... Is>
inline TupleT unpack(char *Data,
                     const std::index_sequence<Is...> & /*1..TupleSize*/) {
  return {get<TupleT>(Data, std::make_index_sequence<Is + 1>{})...};
}

enum class OCLApiKind : uint32_t {
#define _API_OCL(api) api,
#include "xpti_opencl/api.def"
#undef _API_OCL
};

void fillOCLArgs(APICall *call, uint32_t functionID, void *args) {
  struct _cl_icd_dispatch d;
#define _API_OCL(api)                                                          \
  if (functionID == static_cast<uint32_t>(OCLApiKind::api)) {                  \
    using TupleT = typename function_traits<                                   \
        std::remove_pointer_t<decltype(d.api)>>::args_type;                    \
                                                                               \
    TupleT tuple = unpack<TupleT>(                                             \
        static_cast<char *>(args),                                             \
        std::make_index_sequence<std::tuple_size<TupleT>::value>{});           \
    const auto wrapper = [&](auto &...args) { collectArgs(call, args...); };   \
    std::apply(wrapper, tuple);                                                \
    return;                                                                    \
  }

#include "xpti_opencl/api.def"
#undef _API_OCL
}

} // namespace dpcpp_trace::detail
