#include "options.hpp"

#include <iostream>
#include <string_view>

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

options::options(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Use prp info to see available options";
    std::terminate();
  }

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
  } else if (command == "info") {
    mMode = mode::info;
  }
}
