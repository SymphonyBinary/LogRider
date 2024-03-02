#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>
#include <cassert>
#include <algorithm>

/*
------------------------------------------------------------------------------
CHANNEL MESSAGE (always the first caplog message to get displayed per process)
------------------------------------------------------------------------------
1         2            3   4                5             6               7     8     
CAP_LOG : P=4165984483 T=0 CHANNEL-ID=002 : ENABLED=YES : VERBOSITY : 0 : >  >  RENDER_SUB_CHANNEL_A

1 - main delimiter
2 - process timestamp (used to uniquely identify this log to the process).  Remaps to a smaller number in vsix.
3 - relative thread id
4 - channel ID (3 digits)
5 - If the channel is enabled or not
6 - verbosity level
7 - tabs to show channel hierarchical relationship
8 - channel name

------------------------------------------------------------------------------
LOG LINES:
------------------------------------------------------------------------------

1         2            3   4     5  6 7     8           9                                               10
CAP_LOG : P=4293102038 T=0 C=005 :F 3 [25]::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
CAP_LOG : P=4293102038 T=0 C=005 :-> 3 [25] LOG: Testing format = hello 
CAP_LOG : P=4293102038 T=0 C=005 :L 3 [25]::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730

1 - main delimiter
2 - process timestamp (used to uniquely identify this log to the process).  Remaps to a smaller number in vsix.
3 - relative thread id
4 - channel ID (3 digits)
5 - PRIMARY_LOG_BEGIN_DELIMITER (start of block)
    F = start of block
    -> = log within block
    L = end of block
    : = function depth of logged functions (appends as prefix.  Eg. :F, :L, ::F, etc)
6 - per thread unique function idx (monotonic counter)
7 - line in the source file
8 - filename
9 - function name
10 - *this* pointer
*/

namespace {
/**
 * This will match the opening and closing block tags
 * eg. ::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
 * 0 - the full string
 * 1 - filename
 * 2 - Function name (may be truncated if too long)
 * 3 - The object id.  In c++ this is the "this" pointer (or 0 if none)
 **/
std::regex infoStringBlockMatch(
  "::\\[(.*)\\]::\\[(.*)\\] ([0-9a-z]+)",
  std::regex_constants::ECMAScript
);

class WorldState {
public:    
  struct LoggedObject {
    std::string objectId;
    std::unordered_map<std::string, std::unordered_map<int, std::string>> pushedVariables;
  };

  struct StackNode {
    StackNode(
      size_t line, 
      size_t depth, 
      size_t threadId, 
      size_t processId, 
      std::string filename, 
      std::string functionName,
      StackNode* caller)
      : line(line)
      , depth(depth)
      , threadId(threadId)
      , processId(processId)
      , filename(std::move(filename))
      , functionName(std::move(functionName))
      , caller(caller) {}

    const size_t line;
    const size_t depth;
    const size_t threadId;
    const size_t processId;
    const std::string filename;
    const std::string functionName;
    const StackNode* caller;
    std::optional<LoggedObject> loggedObject;
  };

  // should use expected, but that's only in c++23
  // the objectId is address of the object for logs in c++
  enum ExistsPolicy{DontCreateIfNotExist, CreateIfNotExist};
  LoggedObject* getLoggedObject(const std::string& objectId, ExistsPolicy existsPolicy) {
    if (objectId.empty()) {
      return nullptr;
    }

    LoggedObject* retLoggedObject;
    if (auto loggedObjMapIter = mLoggedObjects.begin(); loggedObjMapIter != mLoggedObjects.end()) {
      retLoggedObject = loggedObjMapIter->second.get();
    } else if (existsPolicy == ExistsPolicy::CreateIfNotExist) {
      auto newLoggedObject = std::make_unique<LoggedObject>();
      newLoggedObject->objectId = objectId;
      retLoggedObject = newLoggedObject.get();
      mLoggedObjects.emplace(objectId, std::move(newLoggedObject));
    }

    return retLoggedObject;
  }

  StackNode& pushNewStackNode(
      size_t line,
      size_t depth, 
      size_t threadId, 
      size_t processId, 
      std::string filename, 
      std::string functionName,
      StackNode* caller) {
    size_t stackNodeIdx = mStackNodeArray.size();
    assert(stackNodeIdx == line);

    mStackNodeArray.emplace_back(std::make_unique<StackNode>(
      line, depth, threadId, processId, std::move(filename), std::move(functionName), caller));

    ThreadIdToStackNodeIdxArray& threadIdToStackNodeIdxArray = 
      mProcessIdToThreadIdToStackNodeIdxArray[processId];
    threadIdToStackNodeIdxArray[threadId].emplace_back(stackNodeIdx);
    
    return *mStackNodeArray.back().get();
  }

  StackNode* getStackNodeOnLine(size_t lineNumber) {
    if (lineNumber < mStackNodeArray.size()) {
      return mStackNodeArray[lineNumber].get();
    } else {
      return nullptr;
    }
  }

private:
  std::unordered_map<std::string, std::unique_ptr<LoggedObject>> mLoggedObjects;
  
  using StackNodeArray = std::vector<std::unique_ptr<StackNode>>;
  StackNodeArray mStackNodeArray;

  using StackNodeIdxArray = std::vector<size_t>;
  using ThreadIdToStackNodeIdxArray = std::unordered_map<size_t, StackNodeIdxArray>;
  using ProcessIdToThreadIdToStackNodeIdxArray = std::unordered_map<size_t, ThreadIdToStackNodeIdxArray>;
  ProcessIdToThreadIdToStackNodeIdxArray mProcessIdToThreadIdToStackNodeIdxArray;
};

struct WorldStateWorkingData {
  size_t nextProcessId = 0;
  size_t nextThreadId = 0;
  size_t nextLogLineNumber = 0;
  std::string inputLine;
};

struct OutputState {
  std::string outputText;
};

bool processLogLine(WorldStateWorkingData& workingData, WorldState& worldState, OutputState& outputState) {
  /**
   * This will match all the logs:
   * CAP_LOG_BLOCK, CAP_LOG_BLOCK_NO_THIS, CAP_LOG, CAP_LOG_ERROR, CAP_SET
   * the sub-expressions:
   * 0 - the full string
   * 1 - ProcessId
   * 2 - ThreadId
   * 3 - ChannelId
   * 4 - Indentation
   * 5 - FunctionId
   * 6 - Line number in source code
   * 7 - Info string; the remainder of the string.  Changes depending on log type.
   **/
  std::regex logLineRegex(
    ".*CAP_LOG : P=(.+?) T=(.+?) C=(.+?) (.+?) (.+?) (\\[.+?\\])(.*)",
    std::regex_constants::ECMAScript);

  std::cmatch pieces_match;
  bool matched = std::regex_match (workingData.inputLine.c_str(), pieces_match, logLineRegex);
  if (matched) {
    // structured bindings don't work for regex match :(
    // cannot decompose inaccessible member ‘std::__cxx11::match_results<__gnu_cxx::__normal_iterator<...
    auto&& inputFullMatch = pieces_match[0];
    auto&& inputProcessId = pieces_match[1];   // 2
    auto&& inputThreadId = pieces_match[2];    // 3
    auto&& inputChannelId = pieces_match[3];   // 4
    auto&& inputIndentation = pieces_match[4]; // 5
    auto&& inputFunctionId = pieces_match[5];  // 6
    auto&& inputSourceLine = pieces_match[6];  // 7
    auto&& inputInfoString = pieces_match[7];  // 8

    std::outputIndentation = inputIndentation;
    std::replace(outputIndentation.begin(), outputIndentation.end(), ":", "║");
    std::replace(outputIndentation.begin(), outputIndentation.end(), "F", "║");
    std::replace(outputIndentation.begin(), outputIndentation.end(), "L", "║");
    std::replace(outputIndentation.begin(), outputIndentation.end(), "-", "║");
    std::replace(outputIndentation.begin(), outputIndentation.end(), ">", "║");
  }

  return matched;
}

size_t getFileSize(char* filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  return file.tellg();
}

} // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage: processClog [clogfile.clog]";
    return 0;
  }
  char* inputfilename = argv[1];

  size_t inputFileSize = getFileSize(inputfilename);

  std::ifstream fileStream(inputfilename);

  OutputState output;
  output.outputText.reserve(inputFileSize * 2);

  WorldState worldState;
  WorldStateWorkingData worldWorkingData;

  std::string inputLine;
  while (std::getline(fileStream, inputLine)) {
    worldWorkingData.inputLine = std::move(inputLine);
    if (processLogLine(worldWorkingData, worldState, output)) {
      continue;
    }
  }


  //std::cout << outputText << std::endl;

  return 0;
}

