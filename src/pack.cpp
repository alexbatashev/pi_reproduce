#include "common.hpp"
#include "constants.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

void pack(const options &opts) {
  if (!std::filesystem::exists(opts.input())) {
    std::cerr << "Path does not exist: " << opts.input() << "\n";
    exit(EXIT_FAILURE);
  }

  if (std::filesystem::exists(opts.input() / kPackedDataPath)) {
    std::cerr << "Trace is already packed\n";
    exit(EXIT_FAILURE);
  }

  std::filesystem::create_directory(opts.input() / kPackedDataPath);

  std::ifstream recordFilesConfig{opts.input() / kFilesConfigName};
  json recordFiles;
  recordFilesConfig >> recordFiles;

  json replayMap;

  std::ifstream replayConfigIn{opts.input() / kReplayConfigName};
  json replayConfig;
  replayConfigIn >> replayConfig;
  replayConfigIn.close();

  std::filesystem::path executable{
      replayConfig[kReplayExecutable].get<std::string>()};
  std::filesystem::copy_file(executable, opts.input() / kPackedDataPath / "0");

  replayMap[executable.string()] = "0";

  size_t counter = 1;

  for (auto &element : recordFiles) {
    std::string candString = element.get<std::string>();
    if (candString.starts_with("/dev"))
      continue;
    if (candString.starts_with("/sys"))
      continue;
    if (candString.starts_with("/proc"))
      continue;

    std::filesystem::path candPath{candString};
    if (!std::filesystem::is_regular_file(candPath))
      continue;

    std::string newFileName = std::to_string(counter++);

    std::filesystem::copy_file(candPath,
                               opts.input() / kPackedDataPath / newFileName);
    replayMap[candString] = newFileName;
  }

  std::ofstream replayFilesMapConfig{opts.input() / kReplayFileMapConfigName};
  replayFilesMapConfig << replayMap.dump(2);
  replayFilesMapConfig.close();

  replayConfig[kRecordMode] = kRecordModeFull;

  std::ofstream replayConfigOut{opts.input() / kReplayConfigName};
  replayConfigOut << replayConfig.dump(4);
  replayConfigOut.close();
}
