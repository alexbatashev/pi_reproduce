#include <catch2/catch.hpp>

#include "utils/Tracer.hpp"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

inline constexpr auto kForkTestFilename = "fork_test";
inline constexpr auto kOpenTestFilename = "open_test";
inline constexpr auto kStatTestFilename = "stat_test";
inline constexpr auto kReplaceTestFilename1 = "replace1_test";
inline constexpr auto kReplaceTestFilename2 = "replace2_test";

namespace fs = std::filesystem;
using namespace dpcpp_trace;
using namespace std::chrono_literals;

TEST_CASE("can fork processes", "[NativeTracer]") {
  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    std::ofstream os{fs::temp_directory_path() / kForkTestFilename};
    os << 42;
    os.close();
  };

  NativeTracer tracer;
  tracer.fork(start, false);
  tracer.wait();

  std::ifstream is{fs::temp_directory_path() / kForkTestFilename};
  int n;
  is >> n;
  is.close();
  fs::remove(fs::temp_directory_path() / kForkTestFilename);

  REQUIRE(n == 42);
}

TEST_CASE("can trace files being open", "[NativeTracer]") {
  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    std::ofstream os{fs::temp_directory_path() / kOpenTestFilename};
    os << 42;
    os.close();
  };

  bool test = false;

  NativeTracer tracer;
  tracer.onFileOpen([&test](std::string_view filename, const OpenHandler &) {
    if (filename.ends_with(kOpenTestFilename))
      test = true;
  });
  tracer.fork(start, false);
  tracer.wait();

  std::ifstream is{fs::temp_directory_path() / kOpenTestFilename};
  int n;
  is >> n;
  is.close();
  fs::remove(fs::temp_directory_path() / kOpenTestFilename);

  REQUIRE(test);
  REQUIRE(n == 42);
}

TEST_CASE("can trace files being stat", "[NativeTracer]") {
  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    std::ofstream os{fs::temp_directory_path() / kStatTestFilename};
    os << 42;
    os.close();
    (void)fs::status(fs::temp_directory_path() / kStatTestFilename);
  };

  bool test = false;

  NativeTracer tracer;
  tracer.onStat([&test](std::string_view filename, const StatHandler &) {
    if (filename.ends_with(kStatTestFilename))
      test = true;
  });
  tracer.fork(start, false);
  tracer.wait();

  std::ifstream is{fs::temp_directory_path() / kStatTestFilename};
  int n;
  is >> n;
  is.close();
  fs::remove(fs::temp_directory_path() / kStatTestFilename);

  REQUIRE(test);
  REQUIRE(n == 42);
}

TEST_CASE("can replace filenames", "[NativeTracer]") {
  std::ofstream os{fs::temp_directory_path() / kReplaceTestFilename1};
  os << 10;
  os.close();

  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    std::ofstream os{fs::temp_directory_path() / kReplaceTestFilename1};
    os << 42;
    os.close();
  };

  NativeTracer tracer;
  tracer.onFileOpen([](std::string_view filename, const OpenHandler &h) {
    if (filename.ends_with(kReplaceTestFilename1)) {
      fs::path orig{filename};
      fs::path repl = orig.parent_path() / kReplaceTestFilename2;
      h.replaceFilename(repl.string());
    }
  });
  tracer.fork(start, false);
  tracer.wait();

  std::ifstream is1{fs::temp_directory_path() / kReplaceTestFilename1};
  int n;
  is1 >> n;
  is1.close();
  fs::remove(fs::temp_directory_path() / kReplaceTestFilename1);

  REQUIRE(fs::exists(fs::temp_directory_path() / kReplaceTestFilename2));
  std::ifstream is2{fs::temp_directory_path() / kReplaceTestFilename2};
  int k;
  is2 >> k;
  is2.close();
  fs::remove(fs::temp_directory_path() / kReplaceTestFilename2);

  REQUIRE(k == 42);
  REQUIRE(n == 10);
}

TEST_CASE("can catch signals", "[NativeTracer]") {
  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    std::raise(SIGABRT);
  };
  NativeTracer tracer;
  tracer.fork(start, false);
  int result = tracer.wait();

  REQUIRE(result != 0);
}

TEST_CASE("can capture exit code", "[NativeTracer]") {
  const auto start = []() {
    std::this_thread::sleep_for(30ms);
    exit(42);
  };
  NativeTracer tracer;
  tracer.fork(start, false);
  int result = tracer.wait();

  REQUIRE(result == 42);
}
