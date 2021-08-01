#include <catch2/catch.hpp>
#include <stdexcept>

#include "utils.hpp"

#include <iostream>

TEST_CASE("which finds full path to executable", "[utils]") {
  std::string_view path{getenv("PATH")};

  std::cout << path << std::endl;

  std::string fullPath = "";
  REQUIRE_NOTHROW(fullPath = which(path, "ls"));

  REQUIRE(fullPath == "/usr/bin/ls");
}
