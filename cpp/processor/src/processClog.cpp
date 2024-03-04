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
#include <cstdlib>

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

enum class CapLineType {
  CAPLOG,
  CHANNEL,
  UNKNOWN,
};

enum class CapLogType {
  BLOCK_SCOPE_OPEN,
  BLOCK_SCOPE_CLOSE,
  BLOCK_INNER_LINE,
  UNKNOWN,
};

enum class CapLogInnerType {
  LOG,
  ERROR,
  SET,
  UNKNOWN,
};

struct InputLogLine {
  CapLogType inputLineType = CapLogType::UNKNOWN;
  int inputLineDepth;

  std::string inputFullString;
  std::string inputProcessId;
  std::string inputThreadId;
  std::string inputChannelId;
  std::string inputIndentation;
  std::string inputFunctionId;
  std::string inputSourceFileLine;
  std::string inputInfoString;
};

struct OutputLogTextCommon {
  std::string channelId;
  std::string indentation;
  std::string functionId;
  std::string sourceFileLine;
};

struct OutputLogTextBlock {
  std::string filename;
  std::string functionName;
  std::string objectId;
};

struct OutputLogTextMessage {
  CapLogInnerType innerType;
  std::string innerPayload;
};

struct OutputLogData {
  int lineDepth;
  size_t uniqueProcessId;
  size_t uniqueThreadId;

  OutputLogTextCommon commonLogText;

  CapLogType logLineType = CapLogType::UNKNOWN;

  //logLineType = block open/close, if applicable
  OutputLogTextBlock blockText;

  // logLineType = logs/error/set, if applicable
  OutputLogTextMessage messageText;
};

class WorldState {
public:    
  struct LoggedObject {
    std::string objectId;
    std::unordered_map<std::string, std::unordered_map<int, std::string>> pushedVariables;
  };

  // should rename this, this only represents CapLog types, not channels.
  struct StackNode {
    StackNode(
      int line, 
      OutputLogData&& logData,
      const StackNode* caller)
      : line(line)
      , depth(std::move(logData.lineDepth))
      , uniqueProcessId(std::move(logData.uniqueProcessId))
      , uniqueThreadId(std::move(logData.uniqueThreadId))
      , commonLogText(std::move(logData.commonLogText))
      , logLineType(std::move(logData.logLineType))
      , blockText(std::move(logData.blockText))
      , messageText(std::move(logData.messageText))
      , caller(caller) {}

    const int line;
    const int depth;
    const size_t uniqueProcessId;
    const size_t uniqueThreadId;

    OutputLogTextCommon commonLogText;
    CapLogType logLineType = CapLogType::UNKNOWN;
    //logLineType = block open/close, if applicable
    OutputLogTextBlock blockText;
    // logLineType = logs/error/set, if applicable
    OutputLogTextMessage messageText;

    const StackNode* caller;
    LoggedObject* loggedObject;
  };

  // should use expected, but that's only in c++23
  // the objectId is address of the object for logs in c++
  enum ExistsPolicy{DontCreateIfNotExist, CreateIfNotExist};
  LoggedObject* getLoggedObject(const std::string& objectId, ExistsPolicy existsPolicy) {
    if (objectId.empty()) {
      return nullptr;
    }

    LoggedObject* retLoggedObject;
    if (auto loggedObjMapIter = mLoggedObjects.find(objectId); loggedObjMapIter != mLoggedObjects.end()) {
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
      OutputLogData&& logData,
      const StackNode* caller) {
    size_t stackNodeIdx = mStackNodeArray.size();

    mStackNodeArray.emplace_back(std::make_unique<StackNode>(
      stackNodeIdx, std::move(logData), caller));

    UniqueThreadIdToStackNodeIdxArray& uniqueThreadIdToStackNodeIdxArray = 
      mUniqueProcessIdToUniqueThreadIdToStackNodeIdxArray[logData.uniqueProcessId];
    uniqueThreadIdToStackNodeIdxArray[logData.uniqueThreadId].emplace_back(stackNodeIdx);
    
    return *mStackNodeArray.back().get();
  }

  StackNode* getStackNodeOnLine(size_t lineNumber) {
    if (lineNumber < mStackNodeArray.size()) {
      return mStackNodeArray[lineNumber].get();
    } else {
      return nullptr;
    }
  }

  StackNode* getLastStackNodeForProcessThread(size_t uniqueProcessId, size_t uniqueThreadId) {
    if (auto findThreadArrayIter = mUniqueProcessIdToUniqueThreadIdToStackNodeIdxArray.find(uniqueProcessId);
        findThreadArrayIter != mUniqueProcessIdToUniqueThreadIdToStackNodeIdxArray.end()) {
      if (auto findStackArrayIter = findThreadArrayIter->second.find(uniqueThreadId);
          findStackArrayIter != findThreadArrayIter->second.end()) {
        size_t stackNodeIdx = findStackArrayIter->second.back();
        return mStackNodeArray[stackNodeIdx].get();
      }
    }

    return nullptr;
  }

  size_t getStackNodeArraySize() const {
    return mStackNodeArray.size();
  }

private:
  std::unordered_map<std::string, std::unique_ptr<LoggedObject>> mLoggedObjects;
  
  using StackNodeArray = std::vector<std::unique_ptr<StackNode>>;
  StackNodeArray mStackNodeArray;

  using StackNodeIdxArray = std::vector<size_t>;

  // can probably replace these with 2d vectors.
  using UniqueThreadIdToStackNodeIdxArray = std::unordered_map<size_t, StackNodeIdxArray>;
  using UniqueProcessIdToUniqueThreadIdToStackNodeIdxArray = std::unordered_map<size_t, UniqueThreadIdToStackNodeIdxArray>;
  UniqueProcessIdToUniqueThreadIdToStackNodeIdxArray mUniqueProcessIdToUniqueThreadIdToStackNodeIdxArray;
};


class WorldStateWorkingData {
public: 
  // tracks what we've read so far in the file.
  size_t intputFileLineNumber = 0;
  std::string inputLine;
  
  // out lines don't line up with the input fiels for two reasons:
  // 1 - we filter out any non-cap-log messages
  // 2 - sometimes the input log is missing lines or corrupted.  We try to 
  //     add recovery here.
  size_t nextLogLineNumber = 0;

  CapLineType lineType;

  // maybe use variants on this to better select the right type
  std::unique_ptr<InputLogLine> inputLogLine;
  std::unique_ptr<OutputLogData> outputLogData;
  WorldState::StackNode* prevStackNode = nullptr;

  // todo channel types.
  //
  //
  

  size_t getUniqueProcessIdForInputProcessId(const std::string& inputProcessId) {
    size_t retId;
    if (auto findUniqueProcessIdIter = mProcessToUniqueProcessId.find(inputProcessId); 
        findUniqueProcessIdIter != mProcessToUniqueProcessId.end()) {
      retId = findUniqueProcessIdIter->second;
    } else {
      retId = nextUniqueProcessId++;
      mProcessToUniqueProcessId[inputProcessId] = retId;
      mUniqueProcessIdToInputThreadToUniqueThreadId[retId];
    }

    // std::cout << retId << std::endl;
    return retId;
  }

  size_t getUniqueThreadIdForInputThreadId(size_t uniqueProcessId, const std::string& inputThreadId) {
    size_t retId;
    if (auto findThreadMapIter = mUniqueProcessIdToInputThreadToUniqueThreadId.find(uniqueProcessId);
        findThreadMapIter != mUniqueProcessIdToInputThreadToUniqueThreadId.end()) {
      if (auto findUniqueThreadIdIter = findThreadMapIter->second.find(inputThreadId); 
          findUniqueThreadIdIter != findThreadMapIter->second.end()) {
        retId = findUniqueThreadIdIter->second;
      } else {
        retId = nextUniqueThreadId++;
        findThreadMapIter->second[inputThreadId] = retId;
      }
    } else {
      std::cerr << "Cannot find inputThreadId to UniqueId map for given uniqueProcessId" << std::endl;
      abort();
    }

    // std::cout << retId << std::endl;
    return retId;    
  }

private:
  size_t nextUniqueProcessId = 0;
  std::unordered_map<std::string, size_t> mProcessToUniqueProcessId;

  size_t nextUniqueThreadId = 0;
  std::unordered_map<size_t, std::unordered_map<std::string, size_t>> mUniqueProcessIdToInputThreadToUniqueThreadId;
};

struct OutputState {
  std::string outputText;
};

void failWithAbort(const WorldStateWorkingData& workingData, std::string additionalInfo = "") {
  std::cout << "FAILED TO PROCESS LINE" << std::endl;
  std::cout << "line(" << workingData.intputFileLineNumber << "): "
            << workingData.inputLine << std::endl;
  std::cout << additionalInfo << std::endl;
  std::abort();
}

CapLogType getLineType(const std::string& inputIndentation) {
  CapLogType retType = CapLogType::UNKNOWN;
  std::smatch match;
  if (std::regex_match(inputIndentation, match, std::regex(".*F"))) {
    retType = CapLogType::BLOCK_SCOPE_OPEN;
  } else if (std::regex_match (inputIndentation, match, std::regex(".*L"))) {
    retType = CapLogType::BLOCK_SCOPE_CLOSE;
  } else if (std::regex_match (inputIndentation, match, std::regex(".*>"))) {
    retType = CapLogType::BLOCK_INNER_LINE;
  }

  std::cout << "inputIndendation: " << inputIndentation << std::endl;
  std::cout << "getLineType: " << (size_t)retType << std::endl;
  return retType;
}

int getLineDepth(const std::string& inputIndentation) {
  int retVal = inputIndentation.size();
  std::cout << retVal << std::endl;
  return retVal;
}

std::string replaceIndentationChars (std::string inputIndentation) {
  inputIndentation = std::regex_replace(inputIndentation, std::regex(":+"), "║");
  inputIndentation = std::regex_replace(inputIndentation, std::regex("F"), "╔");
  inputIndentation = std::regex_replace(inputIndentation, std::regex("L"), "╚");
  inputIndentation = std::regex_replace(inputIndentation, std::regex("-"), "╠");
  inputIndentation = std::regex_replace(inputIndentation, std::regex(">"), "╾");
  // std::cout << inputIndentation << std::endl;

  return inputIndentation;
}


void processBlockScopeOpen (
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();
  OutputLogData& outputLogData = *workingData.outputLogData.get();

  int selfDepth = inputLogLine.inputLineDepth;
  int expectedSelfDepth = -1;

  // determine expected depth first, then do fix up if necessary
  // then get the caller node (which might be newly created)
  if (workingData.prevStackNode) {
    switch (workingData.prevStackNode->logLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        // intentional fall-through
      case CapLogType::BLOCK_INNER_LINE:
        expectedSelfDepth = workingData.prevStackNode->depth + 1;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        expectedSelfDepth = workingData.prevStackNode->depth;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockScopeOpen can't determine prev logline type");
    }
  } else {
    expectedSelfDepth = 1;
  }

  if (selfDepth != expectedSelfDepth) {
    // TODO: use fix up method here later.
    std::cout << "selfDepth: " << selfDepth << std::endl;
    std::cout << "expectedSelfDepth: " << expectedSelfDepth << std::endl;
    failWithAbort(workingData, "selfDepth != expectedSelfDepth");
  }

  const WorldState::StackNode* callerStackNode = nullptr;
  if (workingData.prevStackNode) {
    switch (workingData.prevStackNode->logLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        // intentional fall-through
      case CapLogType::BLOCK_INNER_LINE:
        callerStackNode = workingData.prevStackNode;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        callerStackNode = workingData.prevStackNode->caller;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockScopeOpen can't determine prev logline type");
    }
  } else {
    callerStackNode = nullptr;
  }

  std::smatch pieces_match;
  bool matched = std::regex_match(workingData.inputLogLine->inputInfoString, pieces_match, infoStringBlockMatch);
  if(matched) {
    // TODO: Should move this into a similar "input" struct like the initial log line.
    outputLogData.blockText.filename = pieces_match[1];
    outputLogData.blockText.functionName = pieces_match[2];
    outputLogData.blockText.objectId = pieces_match[3];
  } else {
    failWithAbort(workingData, "Unabled to match infoStringBlockMatch with expected block opening line");
  }

  worldState.pushNewStackNode(std::move(outputLogData), callerStackNode);
}

bool processLogLine(
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  std::smatch pieces_match;
  bool matched = std::regex_match (workingData.inputLine, pieces_match, logLineRegex);
  if (matched) {
    workingData.lineType = CapLineType::CAPLOG;
    workingData.inputLogLine = std::make_unique<InputLogLine>();
    workingData.outputLogData = std::make_unique<OutputLogData>();
    InputLogLine& inputLogLine = *workingData.inputLogLine.get();
    OutputLogData& outputLogData = *workingData.outputLogData.get();

    // structured bindings don't work for regex match :(
    // cannot decompose inaccessible member ‘std::__cxx11::match_results<__gnu_cxx::__normal_iterator<...
    inputLogLine.inputFullString = pieces_match[0];
    inputLogLine.inputProcessId = pieces_match[1];   // 2
    inputLogLine.inputThreadId = pieces_match[2];    // 3
    inputLogLine.inputChannelId = pieces_match[3];   // 4
    inputLogLine.inputIndentation = pieces_match[4]; // 5
    inputLogLine.inputFunctionId = pieces_match[5];  // 6
    inputLogLine.inputSourceFileLine = pieces_match[6];  // 7
    inputLogLine.inputInfoString = pieces_match[7];  // 8
    inputLogLine.inputLineType = getLineType(inputLogLine.inputIndentation);
    inputLogLine.inputLineDepth = getLineDepth(inputLogLine.inputIndentation);

    outputLogData.lineDepth = inputLogLine.inputLineDepth;
    outputLogData.uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(inputLogLine.inputProcessId);
    outputLogData.uniqueThreadId = workingData.getUniqueThreadIdForInputThreadId(outputLogData.uniqueProcessId, inputLogLine.inputThreadId);
    outputLogData.commonLogText.channelId = inputLogLine.inputChannelId;
    outputLogData.commonLogText.indentation = replaceIndentationChars(inputLogLine.inputIndentation);
    outputLogData.commonLogText.functionId = inputLogLine.inputFunctionId;
    outputLogData.commonLogText.sourceFileLine = inputLogLine.inputSourceFileLine;
    outputLogData.logLineType = inputLogLine.inputLineType;

    // inputLogLine.inputLineDepth can resolve to a block open/close or inner log/error/set
    workingData.prevStackNode = worldState.getLastStackNodeForProcessThread(outputLogData.uniqueProcessId,
      outputLogData.uniqueThreadId);

    switch (inputLogLine.inputLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        processBlockScopeOpen(workingData, worldState);
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        break;
      case CapLogType::BLOCK_INNER_LINE:
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "Unknown input log line type");
    }

    // // Determine who is the "caller" if there is one.  The caller is the 
    // // previous logged line if it has less depth than this one.
    // //
    // // If the different in depth between the last line and this exceeds 1, then 
    // // a log line failed to get output and/or was corrupted.  We'll
    // // fill that in with ???
    // WorldState::StackNode* callerStackNode = nullptr;
    // WorldState::StackNode* prevStackNode = worldState.getLastStackNodeForProcessThread(outputUniqueProcessId,
    //   outputUniqueThreadId);
    // if (prevStackNode) {
    //   // if this indentation depth is larger, we're "in" the previous stackNode's "scope"
    //   if (inputIndentationDepth > prevStackNode->depth) {
    //     // if difference is larger than 1, some logs somehow didn't print so we need to make up the difference.
    //     // these should be "new" blocks to open, but we don't have a unique key for the "this pointer"
    //     // TODO: create a new map from "inputObjectId" to "uniqueObjectId".  This way we can insert extra
    //     // objectids if needed, such as in this case.
    //     while ((inputIndentationDepth - 1) != prevStackNode->depth) {
    //       std::cerr << "unexpected jump in depth on line# " << workingData.intputFileLineNumber 
    //       << " in input file. Output file is on line# " << worldState.getStackNodeArraySize();
    //       prevStackNode = &worldState.pushNewStackNode(
    //           prevStackNode->depth + 1, prevStackNode->uniqueThreadId, prevStackNode->uniqueProcessId, prevStackNode->channelId,
    //           "???", "???", prevStackNode);
    //       std::abort; // abort for now              
    //     }
    //     callerStackNode = prevStackNode;
    //   // conversely, if the indentation is smaller, the previous scope was our "child".  
    //   // so our caller is their great-grandparent.
    //   } else if (inputIndentationDepth < prevStackNode->depth) {
    //     // WorldState::StackNode* ancestor = (prevStackNode->caller ? prevStackNode->caller->caller : nullptr);
        
    //     // // if difference is larger than 1, some logs somehow didn't print so we need to make up the difference.
    //     // while ((inputIndentationDepth + 1) != prevStackNode->depth) {
    //     //   std::cerr << "unexpected jump in depth on line# " << workingData.intputFileLineNumber 
    //     //   << " in input file. Output file is on line# " << worldState.getStackNodeArraySize();
          
    //     //   // We're missing a line so we just use the previous node's processid, threadid, and channelid because we have to guess.
    //     //   // the log needs to look like an end of block.
    //     //   prevStackNode = worldState.pushNewStackNode(
    //     //       prevStackNode->depth - 1, prevStackNode->uniqueThreadId, prevStackNode->uniqueProcessId, prevStackNode->channelId,
    //     //       "???", "???", ancestor);
    //     //   if (ancestor) {
    //     //     prevStackNode->loggedObject = ancestor->loggedObject;
    //     //   }            
    //     //   ancestor = (prevStackNode->caller ? prevStackNode->caller->caller : nullptr);
    //     // }
    //     // callerStackNode = ancestor;
    //   }
    
    //   // 

    //   // if our depth is teh same, we need to inherit the same parent 
    // }

  }

  return matched;
}



// void adjustToExpectedDepth(expected depth)

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

  // OutputState output;
  // output.outputText.reserve(inputFileSize * 2);

  WorldState worldState;
  WorldStateWorkingData worldWorkingData;

  //TODO: progress bar
  std::string inputLine;
  while (std::getline(fileStream, inputLine)) {
    worldWorkingData.inputLine = std::move(inputLine);
    if (processLogLine(worldWorkingData, worldState)) {
      continue;
    }

    ++worldWorkingData.intputFileLineNumber;
  }


  //std::cout << outputText << std::endl;

  return 0;
}

