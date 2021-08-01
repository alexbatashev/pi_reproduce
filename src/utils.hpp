#pragma once

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

inline std::string which(std::string_view path, std::string_view executable) {
  const std::string separator = ":";
  size_t lastSeparator = 0;
  bool done = false;
  do {
    size_t nextSeparator = path.find(separator, lastSeparator);
    size_t length = 0;
    if (nextSeparator == std::string::npos) {
      length = path.size() - lastSeparator;
      done = true;
    } else {
      length = nextSeparator - lastSeparator;
    }
    auto cand =
        std::filesystem::path(path.substr(lastSeparator, length)) / executable;
    std::cout << "Cand is " << cand << std::endl;
    if (std::filesystem::exists(cand)) {
      return cand.string();
    }

    lastSeparator = nextSeparator + 1;
  } while (!done);
  throw std::runtime_error("Executable not found " + std::string{executable});
}
