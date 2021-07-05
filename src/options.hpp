#pragma once

#include <filesystem>
#include <string>
#include <vector>

class options {
public:
  enum class mode { record, replay, print, info };
  enum class print_group_by { none, thread };
  options(int argc, char *argv[]);

  mode command() const noexcept { return mMode; }

  std::filesystem::path input() const noexcept { return mInput; }
  std::filesystem::path output() const noexcept { return mOutput; }

  print_group_by print_group() const noexcept { return mPringGroup; }

  bool verbose() const noexcept { return mVerbose; }

  bool performance_summary() const noexcept { return mPrintPerformanceSummary; }

  const std::vector<std::string> &args() const noexcept { return mArguments; }

  std::filesystem::path location() const noexcept {
    return mExecutablePath.parent_path();
  }

private:
  void parseRecordOptions(int argc, char *argv[]);
  void parseReplayOptions(int argc, char *argv[]);
  void parsePrintOptions(int argc, char *argv[]);

  std::filesystem::path mExecutablePath;
  std::filesystem::path mInput;
  std::filesystem::path mOutput;
  std::vector<std::string> mArguments;
  mode mMode;
  print_group_by mPringGroup = print_group_by::none;
  bool mPrintPerformanceSummary = false;
  bool mVerbose = false;
};
