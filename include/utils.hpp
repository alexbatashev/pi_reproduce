#pragma once

#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <cstring>

class Tracer;

enum class exit_code { none, success, fail };

std::string which(std::string_view executable);

exit_code exec(std::string_view executable, std::span<std::string> args,
               std::span<std::string> env, Tracer &tracer);

template <class To, class From>
inline
typename std::enable_if_t<
    sizeof(To) == sizeof(From) &&
    std::is_trivially_copyable_v<From> &&
    std::is_trivially_copyable_v<To>,
    To>
bit_cast(const From& src) noexcept {
    static_assert(std::is_trivially_constructible_v<To>);
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}
