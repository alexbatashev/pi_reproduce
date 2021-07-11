#pragma once

#include <cstring>
#include <iostream>
#include <ostream>
#include <type_traits>

template <typename T> void single_write(std::ostream &os, T data) {
  if constexpr (std::is_same_v<const char *, std::remove_reference_t<T>>) {
    os.write(data, strlen(data));
    os.put('\n');
  } else {
    os.write(reinterpret_cast<const char *>(&data), sizeof(T));
  }
}

template <typename... Ts> struct write_helper;

template <typename T, typename... Rest> struct write_helper<T, Rest...> {
  static void write(std::ostream &os, T data, Rest... args) {
    single_write(os, data);
    write_helper<Rest...>::write(os, args...);
  }
};

template <typename T> struct write_helper<T> {
  static void write(std::ostream &os, T data) { single_write(os, data); }
};

template <typename... Ts> void write(std::ostream &os, Ts... args) {
  write_helper<Ts...>::write(os, args...);
}

template <typename... Ts> void writeSimple(std::ostream &os, Ts... args) {
  size_t totalSize = (sizeof(Ts) + ...);
  size_t numOutputs = 0;

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));

  write_helper<Ts...>::write(os, args...);
}

template <typename... Ts>
void writeWithOutput(std::ostream &os, size_t numOutputs, Ts... args) {
  size_t totalSize = (sizeof(Ts) + ...);

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));

  write_helper<Ts...>::write(os, args...);
}

template <typename T1, typename T2>
void writeWithInfo(std::ostream &os, T1 obj, T2 param, size_t size, void *value,
                   size_t *retSize) {
  size_t totalSize = sizeof(T1) + sizeof(T2) + 3 * sizeof(size_t);
  size_t numOutputs = 1;

  os.write(reinterpret_cast<const char *>(&numOutputs), sizeof(size_t));
  os.write(reinterpret_cast<const char *>(&totalSize), sizeof(size_t));
  write_helper<T1, T2, size_t, void *, size_t *>::write(os, obj, param, size,
                                                        value, retSize);

  if (value != nullptr) {
    os.write(reinterpret_cast<const char *>(&size), sizeof(size_t));
    os.write(static_cast<const char *>(value), size);
  }
  if (retSize != nullptr) {
    size_t outSize = sizeof(size_t);
    os.write(reinterpret_cast<const char *>(&outSize), sizeof(size_t));
    os.write(reinterpret_cast<const char *>(retSize), outSize);
  }
}
