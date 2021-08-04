#include <catch2/catch.hpp>
#include <stdexcept>

#include "utils.hpp"

#include <iostream>

TEST_CASE("which finds full path to executable", "[utils]") {
  std::string fullPath = "";
  REQUIRE_NOTHROW(fullPath = which("ls"));

  REQUIRE(fullPath == "/usr/bin/ls");
}
