#include <catch2/catch.hpp>
#include <stdexcept>

#include "options.hpp"

TEST_CASE("info doesn't take any parameters", "[info]") {
  std::array<const char *, 1> env = {nullptr};
  SECTION("has extra arguments") {
    std::array<const char *, 3> testArgs = {"prog", "info", "-foo"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char **>(testArgs.data()),
                   const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
  SECTION("no extra arguments") {
    std::array<const char *, 2> testArgs = {"prog", "info"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char **>(testArgs.data()),
                   const_cast<char **>(env.data())};
      REQUIRE(opts.command() == options::mode::info);
    };
    REQUIRE_NOTHROW(run());
  }
}
