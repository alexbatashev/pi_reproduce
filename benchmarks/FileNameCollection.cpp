#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

std::string randomString() {
  std::string str(
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

  std::random_device rd;
  std::mt19937 generator(rd());

  std::shuffle(str.begin(), str.end(), generator);

  return str.substr(0, generator() % 32);
}

static void benchRandomString(benchmark::State &state) {
  for (auto _ : state)
    benchmark::DoNotOptimize(randomString());
}

BENCHMARK(benchRandomString);

static void insertJson(benchmark::State &state) {
  json j;
  for (auto _ : state)
    j[randomString()] = 1;
}

BENCHMARK(insertJson);

static void insertMap(benchmark::State &state) {
  std::unordered_map<std::string, int> m;
  for (auto _ : state)
    m[randomString()] = 1;
}

BENCHMARK(insertMap);

static void insertVector(benchmark::State &state) {
  std::vector<std::pair<std::string, int>> m;
  for (auto _ : state)
    m.emplace_back(randomString(), 1);
}

BENCHMARK(insertVector);
