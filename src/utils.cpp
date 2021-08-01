#include "utils.hpp"
#include "fork.hpp"
#include "trace.hpp"

#include <algorithm>
#include <iterator>

std::string which(std::string_view executable) {
  std::string_view path{getenv("PATH")};

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

exit_code exec(std::string_view executable, std::span<std::string> args,
               std::span<std::string> env, Tracer &tracer) {
  const auto toCString = [](const std::string &str) { return str.c_str(); };

  std::vector<const char *> cArgs;
  cArgs.reserve(args.size() + 1);
  std::transform(args.begin(), args.end(), std::back_inserter(cArgs),
                 toCString);
  cArgs.push_back(nullptr);

  std::vector<const char *> cEnv;
  cEnv.reserve(env.size());
  std::transform(env.begin(), env.end(), std::back_inserter(cEnv), toCString);
  cEnv.push_back(nullptr);

  const auto start = [&]() {
    traceMe();
    auto err =
        execve(executable.data(), const_cast<char *const *>(cArgs.data()),
               const_cast<char *const *>(cEnv.data()));
    if (err) {
      std::cerr << "Unexpected error while running executable: " << errno
                << "\n";
    }
  };

  pid child = fork(start);

  tracer.attach(child);

  int ret = tracer.run();

  return ret == 0 ? exit_code::success : exit_code::fail;
}
