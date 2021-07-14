#include "trace_reader.hpp"

#include <iostream>

RecordHeader readHeader(std::ifstream &is) {
  RecordHeader header{};
  is.read(reinterpret_cast<char *>(&header), sizeof(header));

  return header;
}

Record getNextRecord(std::ifstream &is, std::string threadId,
                     bool readMemObjects) {
  RecordHeader header = readHeader(is);

  if (is.eof()) {
    return {};
  }

  Record record{};
  record.functionId = header.functionId;
  record.backend = header.backend;
  record.start = header.start;
  record.end = header.end;

  record.argsData.resize(header.totalSize);

  is.read(record.argsData.data(), record.argsData.size());

  record.outputs.resize(header.numOutputs);
  for (size_t i = 0; i < header.numOutputs; i++) {
    size_t dataSize;
    is.read(reinterpret_cast<char *>(&dataSize), sizeof(size_t));
    if (readMemObjects) {
      MemObject obj;
      obj.data.resize(dataSize);
      is.read(obj.data.data(), dataSize);
      record.outputs[i] = std::move(obj);
    } else {
      is.seekg(dataSize, std::ios_base::cur);
    }
  }

  is.read(reinterpret_cast<char *>(&record.result), sizeof(pi_result));

  record.threadId = threadId;

  return record;
}
