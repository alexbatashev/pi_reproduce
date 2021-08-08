#include "common.hpp"
#include "constants.hpp"
#include "utils/Compression.hpp"
#include "utils/MappedFile.hpp"
#include "utils/MemoryView.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace dpcpp_trace;

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

  if (!opts.output().empty()) {
    std::ofstream os{opts.output(), std::ios::binary};
    char version = 0;
    os.write(&version, sizeof(char));

    Compression comp;

    for (auto &p : fs::recursive_directory_iterator(opts.input())) {
      auto rel = fs::relative(p, opts.input());
      std::string pathStr = rel.string();
      if (fs::is_directory(p)) {
        char kind = 0;
        os.write(&kind, sizeof(char));
        uint64_t size = pathStr.size();
        os.write(reinterpret_cast<char *>(&size), sizeof(size));
        os.write(pathStr.data(), pathStr.size());
      } else if (fs::is_regular_file(p)) {
        char kind = 1;
        os.write(&kind, sizeof(char));
        uint64_t size = pathStr.size();
        os.write(reinterpret_cast<char *>(&size), sizeof(size));
        os.write(pathStr.data(), pathStr.size());

        MappedFile mapping{p};
        const auto compBuf = comp.compress(mapping);
        uint64_t length = compBuf.size();
        os.write(reinterpret_cast<char *>(&length), sizeof(uint64_t));
        os.write(compBuf.as<char>(), compBuf.size());
        if (pathStr == "replay_config.json") {
          std::cout << "Packed size " << compBuf.size();
          auto dec =
              comp.uncompress(MemoryView{compBuf.data(), compBuf.size()});
        }
      }
    }
    os.close();
  }
}
