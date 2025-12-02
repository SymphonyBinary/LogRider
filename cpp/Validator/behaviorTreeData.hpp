#pragma once

#include "ingest.hpp"

// Persistent state

struct ObjectData {
  std::string className;
  size_t processId;
  size_t threadId;
};

struct TupleBuffers {
  std::vector<std::vector<std::string>> data;
};

struct ProcessState {
  std::unordered_map<std::string, std::vector<ObjectData>> idToObjects;
  std::unordered_map<std::string, TupleBuffers> idToTupleBuffer;
};

struct BehaviorTreeState {
  std::unordered_map<size_t, ProcessState> processIdToState;
  std::ofstream* validationOStream = nullptr;
  int lineIndex = 0;
};

// Transient state

struct FuncString {
  bool valid = false;
  std::string name = "";
  std::string className = "";
  bool isCtor = false;
  bool isDtor = false;
};

struct KVP {
  std::string key;
  std::string value;
};

struct ScanLine {
  bool valid = false;
  std::string label;
  std::vector<KVP> kvps;
};

struct CurrentLine {
  const StackNode& node;

  // only valid when (node.logLineType == CapLogType::BLOCK_SCOPE_OPEN or BLOCK_SCOPE_CLOSE)  
  FuncString funcString;

  // only valid when (node.logLineType == CapLogType::BLOCK_INNER_LINE)
  ScanLine scanLine;
};