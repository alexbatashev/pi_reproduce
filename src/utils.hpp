#pragma once

#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

class Tracer;

enum class exit_code { none, success, fail };

std::string which(std::string_view executable);

exit_code exec(std::string_view executable, std::span<std::string> args,
               std::span<std::string> env, Tracer &tracer);
