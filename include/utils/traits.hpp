#pragma once

namespace dpcpp_trace {
template <typename T> struct function_traits {};

template <typename Ret, typename... Args> struct function_traits<Ret(Args...)> {
  using ret_type = Ret;
  using args_type = std::tuple<Args...>;
};
} // namespace dpcpp_trace
