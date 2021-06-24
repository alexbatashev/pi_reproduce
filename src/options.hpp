#pragma once

#include <string>
#include <vector>
#include <filesystem>

class options {
public:
  enum class mode {
    record,
    replay,
    print,
    info
  };
  options(int argc, char *argv[]);

  mode command() const noexcept { return mMode; }

  std::filesystem::path input() const noexcept { return mInput; }
  std::filesystem::path output() const noexcept { return mOutput; }

  const std::vector<std::string> &args() const noexcept { return mArguments; }
private:
  void parseRecordOptions(int argc, char *argv[]);
  void parseReplayOptions(int argc, char *argv[]);
  void parsePrintOptions(int argc, char *argv[]);
  std::filesystem::path mInput;
  std::filesystem::path mOutput;
  std::vector<std::string> mArguments;
  mode mMode;
};
