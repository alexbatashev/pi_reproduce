#pragma once

#include <cstdint>
#include <functional>
#include <mutex>

namespace dpcpp_trace {
class TLSKey;

namespace detail {
void tls_initialize(TLSKey &key, const std::function<void(void *)> &cb);
void *tls_get(const TLSKey &key);
} // namespace detail

void tls_set(const TLSKey &key, const void *data);

class TLSKey {
private:
  friend void detail::tls_initialize(TLSKey &,
                                     const std::function<void(void *)> &);
  friend void *detail::tls_get(const TLSKey &);
  friend void tls_set(const TLSKey &, const void *);
  uint64_t mKey;
  std::once_flag mOnce;
  std::function<void(void*)> cb;
};

template <typename T>
inline void tls_initialize(TLSKey &key, const std::function<void(T *)> &cb) {
  detail::tls_initialize(key, [cb](void *data) { cb(static_cast<T *>(data)); });
}

template <typename T> inline T *tls_get(const TLSKey &key) {
  return static_cast<T *>(detail::tls_get(key));
}
} // namespace dpcpp_trace
