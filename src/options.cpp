#include "options.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>

static void parseInfoOptions(int argc, char *argv[]) {
  (void)argv;

  if (argc > 2) {
    throw std::runtime_error("info does not take any arguments");
  }
}

void options::parseRecordOptions(int argc, char *argv[]) {
  int i = 2;
  bool hasExtraOpts = false;

  while (i < argc) {
    std::string_view opt{argv[i]};

    if (opt[0] != '-' && mInput.empty()) {
      mInput = opt;
    } else if (opt == "--") {
      hasExtraOpts = true; i++; break;
    } else if ((opt == "--output" || opt == "-o") && mOutput.empty()) {
      if (i + 1 >= argc) {
        throw std::runtime_error("--output requires an argument\n");
      }
      mOutput = argv[++i];
    } else if (opt == "--override" && !mRecordOverrideTrace) {
      mRecordOverrideTrace = true;
    } else if ((opt == "--skip-mem-objects" || opt == "-s") &&
               !mRecordSkipMemObjs) {
      mRecordSkipMemObjs = true;
    } else if (opt == "--no-fork" && !mNoFork) {
      mNoFork = true;
    } else {
      throw std::runtime_error(std::string("unrecognized option ") + std::string(argv[i]));
    }

    i++;
  }

  if (hasExtraOpts) {
    for (int k = i; k < argc; k++) {
      mArguments.emplace_back(argv[k]);
    }
  }

  if (mInput.empty()) {
    throw std::runtime_error("input is required");
  }
  if (mOutput.empty()) {
    throw std::runtime_error("output is required");
  }
}

void options::parseReplayOptions(int argc, char *argv[]) {
  int i = 2;
  bool hasExtraOpts = false;

  while (i < argc) {
    std::string_view opt{argv[i]};

    if (opt[0] != '-') {
      mInput = opt;
    } else if (opt == "--") {
      hasExtraOpts = true;
      i++;
      break;
    } else if (opt == "--output" || opt == "-o" || opt == "-t") {
      if (opt == "--output" || opt == "-t") {
        std::clog << "--output and -o will be deprecated for replay, use -t "
                     "instead\n";
      }
      if (i + 1 >= argc) {
        std::cerr << opt << " requires an argument\n";
        std::terminate();
      }
      mOutput = argv[++i];
    } else if (opt == "--no-fork" && !mNoFork) {
      mNoFork = true;
    } else if ((opt == "--print-only" || opt == "-p") && !mPrintOnly) {
      mPrintOnly = true;
    }

    i++;
  }

  if (hasExtraOpts) {
    for (int k = i; k < argc; k++) {
      mArguments.emplace_back(argv[k]);
    }
  }

  if (mInput.empty()) {
    std::cerr << "input is required\n";
    std::terminate();
  }
}

void options::parsePrintOptions(int argc, char *argv[]) {
  int i = 2;
  bool hasExtraOpts = false;

  while (i < argc) {
    std::string_view opt{argv[i]};

    if (opt[0] != '-') {
      mInput = opt;
    } else if (opt == "--group") {
      std::string_view group{argv[++i]};
      if (group == "none") {
        mPringGroup = print_group_by::none;
      } else if (group == "thread") {
        mPringGroup = print_group_by::thread;
      } else {
        std::cerr << "Expected none or thread for --group argument. Got "
                  << group << "\n";
        std::terminate();
      }
    } else if (opt == "--verbose") {
      mVerbose = true;
    } else if (opt == "--perf") {
      mPrintPerformanceSummary = true;
    }

    i++;
  }

  if (mInput.empty()) {
    std::cerr << "input is required\n";
    std::terminate();
  }
}

void options::parsePackOptions(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "pack accepts only one option\n";
    exit(EXIT_FAILURE);
  }

  mInput = argv[2];
}

options::options(int argc, char *argv[], char *env[]) {
  if (argc < 2) {
    std::cerr << "Use dpcpp_trace info to see available options";
    std::terminate();
  }

  size_t i = 0;
  while (env[i] != nullptr) {
    mEnvVars.emplace_back(env[i++]);
  }

  std::array<char, 1024> buf;
  readlink("/proc/self/exe", buf.data(), buf.size());
  mExecutablePath = std::filesystem::path{buf.data()};

  std::string_view command(argv[1]);

  if (command == "record") {
    mMode = mode::record;
    parseRecordOptions(argc, argv);
  } else if (command == "replay") {
    mMode = mode::replay;
    parseReplayOptions(argc, argv);
  } else if (command == "print") {
    mMode = mode::print;
    parsePrintOptions(argc, argv);
  } else if (command == "info" || command == "--help" || command == "-h") {
    mMode = mode::info;
    parseInfoOptions(argc, argv);
  } else if (command == "pack") {
    mMode = mode::pack;
    parsePackOptions(argc, argv);
  }
}
