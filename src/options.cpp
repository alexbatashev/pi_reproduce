#include "options.hpp"

#include <iostream>
#include <string_view>
#include <unistd.h>

static void parseInfoOptions(int argc, const char *argv[]) {
  (void)argv;

  if (argc > 2) {
    std::cerr << "info does not accept arguments";
    std::terminate();
  }
}

void options::parseRecordOptions(int argc, char *argv[]) {
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
    } else if (opt == "--output" || opt == "-o") {
      if (i + 1 >= argc) {
        std::cerr << "--output requires an argument\n";
        std::terminate();
      }
      mOutput = argv[++i];
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
  if (mOutput.empty()) {
    std::cerr << "output is required\n";
    std::terminate();
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
    } else if (opt == "--output" || opt == "-o") {
      if (i + 1 >= argc) {
        std::cerr << "--output requires an argument\n";
        std::terminate();
      }
      mOutput = argv[++i];
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
  if (mOutput.empty()) {
    std::cerr << "output is required\n";
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
  }
}
