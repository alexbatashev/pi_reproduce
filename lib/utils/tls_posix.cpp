#include "utils/tls.hpp"

#include <cstdint>
#include <pthread.h>
#include <stdexcept>

static_assert(sizeof(pthread_key_t) <= sizeof(uint64_t));

namespace dpcpp_trace {
namespace detail {
struct TLSValue {
  const std::function<void(void *)> &cb;
  void *data;
};

static void tls_cb(void *val) {
  auto *value = static_cast<TLSValue *>(val);
  if (value != nullptr) {
    value->cb(value->data);
  }
}

void tls_initialize(TLSKey &key, const std::function<void(void *)> &cb) {
  std::call_once(key.mOnce, [&key, &cb]() {
    auto *pkey = reinterpret_cast<pthread_key_t *>(&key.mKey);
    int res = pthread_key_create(pkey, tls_cb);
    if (res != 0)
      throw std::runtime_error("Failed to initialize TLS");
    key.cb = cb;
    auto *value = new TLSValue{key.cb, nullptr};
    res = pthread_setspecific(*pkey, value);
    if (res != 0)
      throw std::runtime_error("Failed to initialize TLS");
  });
}

void *tls_get(const TLSKey &key) {
  const auto *pkey = reinterpret_cast<const pthread_key_t *>(&key.mKey);
  auto val = static_cast<TLSValue*>(pthread_getspecific(*pkey));
  if (val)
    return val->data;
  return nullptr;
}
} // namespace detail

void tls_set(const TLSKey &key, const void *data) {
  const auto *pkey = reinterpret_cast<const pthread_key_t *>(&key.mKey);
  auto val = static_cast<detail::TLSValue*>(pthread_getspecific(*pkey));
  if (val == nullptr) {
    val = new detail::TLSValue{key.cb, const_cast<void*>(data)};
    int res = pthread_setspecific(*pkey, val);
    if (res != 0) {
      throw std::runtime_error("Failed to set value");
    }
  } else {
    val->data = const_cast<void*>(data);
  }
}
} // namespace dpcpp_trace
