#include <catch2/catch.hpp>
#include <stdexcept>

#include "options.hpp"

TEST_CASE("record requires input and output", "[record]") {
  std::array<const char*, 1> env = {nullptr};
  SECTION("has input and output") {
    std::array<const char*, 5> testArgs = {"prog", "record", "-o", "test", "input"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
      REQUIRE(opts.input().string() == "input");
      REQUIRE(opts.output().string() == "test");
    };
    REQUIRE_NOTHROW(run());
  }
  SECTION("has input only") {
    std::array<const char*, 3> testArgs = {"prog", "record", "input"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
  SECTION("has two inputs") {
    std::array<const char*, 6> testArgs = {"prog", "record", "-o", "out","input1", "input2"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
  SECTION("has only output") {
    std::array<const char*, 4> testArgs = {"prog", "record", "-o", "output"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
  SECTION("has two outputs") {
    std::array<const char*, 7> testArgs = {"prog", "record", "-o", "output", "-o", "test", "input"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
}

TEST_CASE("optional arguments are handled correctly", "[record]") {
  std::array<const char*, 1> env = {nullptr};
  SECTION("has --skip-mem-objects") {
    std::array<const char*, 6> testArgs = {"prog", "record", "-o", "test", "--skip-mem-objects", "input"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
      REQUIRE(opts.input().string() == "input");
      REQUIRE(opts.output().string() == "test");
      REQUIRE(opts.record_skip_mem_objects());
    };
    REQUIRE_NOTHROW(run());
  }
  SECTION("has --skip-mem-objects twice") {
    std::array<const char*, 7> testArgs = {"prog", "record", "-o", "test", "--skip-mem-objects", "--skip-mem-objects", "input"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
    };
    REQUIRE_THROWS_AS(run(), std::runtime_error);
  }
  SECTION("has extra args") {
    std::array<const char*, 8> testArgs = {"prog", "record", "-o", "test", "input", "--", "--foo", "--bar"};
    const auto run = [&]() {
      options opts{testArgs.size(), const_cast<char**>(testArgs.data()), const_cast<char **>(env.data())};
      REQUIRE(opts.input().string() == "input");
      REQUIRE(opts.output().string() == "test");
      REQUIRE(opts.args().size() == 2);
    };
    REQUIRE_NOTHROW(run());
  }
}
