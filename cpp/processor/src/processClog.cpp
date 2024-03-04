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
#include <cmath> // for progress bar

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
 * This will match all the channel logs
 * the sub-expressions:
 * 0 - the full string
 * 1 - ProcessId
 * 2 - ThreadId
 * 3 - ChannelId
 * 4 - Enabled
 * 5 - VerbosityLevel
 * 6 - ChannelName
 **/
std::regex channelLineRegex(
  ".*CAP_LOG : P=(.+?) T=(.+?) CHANNEL-ID=(.+?) : ENABLED=(.+?) : VERBOSITY=(.+?) : (.+?)",
  std::regex_constants::ECMAScript);


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
  std::regex_constants::ECMAScript);

/**
 * This will match the opening and closing block tags
 * eg. ::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
 * 0 - the full string
 * 1 - inner type: LOG/ERROR/SET
 * 2 - inner message
 **/
std::regex infoStringInnerMatch(
  " (.*?): (.*)",
  std::regex_constants::ECMAScript);

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
  // CapLogInnerType innerType; //may need later...
  std::string innerTypeString; //x-macros this instead.
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

struct LoggedObject {
  std::string objectId;
  std::unordered_map<std::string, std::unordered_map<int, std::string>> pushedVariables;
};

struct ChannelLine {
  std::string fullString;
  size_t uniqueProcessId;
  size_t uniqueThreadId;
  std::string channelId;
  std::string enabled;
  std::string verbosityLevel;
  std::string channelName;
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

class WorldState {
public:
  using StackNodeArray = std::vector<std::unique_ptr<StackNode>>;
  using ChannelArray = std::vector<std::unique_ptr<ChannelLine>>;

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

  const StackNodeArray& getNodeArray() const {
    return mStackNodeArray;
  }

  ChannelLine& pushChannelLine(ChannelLine&& channelLine) {
    mChannelArray.emplace_back(std::make_unique<ChannelLine>(std::move(channelLine)));
    return *mChannelArray.back().get();
  }

  const ChannelArray& getChannelArray() const {
    return mChannelArray;
  }

private:
  std::unordered_map<std::string, std::unique_ptr<LoggedObject>> mLoggedObjects;
  
  StackNodeArray mStackNodeArray;
  ChannelArray mChannelArray;

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
  StackNode* prevStackNode = nullptr;

  // todo channel types.
  std::unique_ptr<ChannelLine> channelLine; 

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
  std::ofstream outputFileStream;
};

void failWithAbort(const WorldStateWorkingData& workingData, std::string additionalInfo = "") {
  std::cerr << "FAILED TO PROCESS LINE" << std::endl;
  std::cerr << "line(" << workingData.intputFileLineNumber << "): "
            << workingData.inputLine << std::endl;
  std::cerr << additionalInfo << std::endl;
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

  // std::cout << "inputIndendation: " << inputIndentation << std::endl;
  // std::cout << "getLineType: " << (size_t)retType << std::endl;
  return retType;
}

int getLineDepth(const std::string& inputIndentation) {
  int retVal = inputIndentation.size();
  // std::cout << retVal << std::endl;
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
        expectedSelfDepth = workingData.prevStackNode->depth + 1;
        break;
      case CapLogType::BLOCK_INNER_LINE:
        expectedSelfDepth = workingData.prevStackNode->depth;
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

  if ((selfDepth != expectedSelfDepth) || (expectedSelfDepth < 1)) {
    // TODO: use fix up method here later.
    std::cerr << "selfDepth: " << selfDepth << std::endl;
    std::cerr << "expectedSelfDepth: " << expectedSelfDepth << std::endl;
    failWithAbort(workingData, "selfDepth != expectedSelfDepth || expectedSelfDepth < 1");
  }

  const StackNode* callerStackNode = nullptr;
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

void processBlockScopeClose (
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
        expectedSelfDepth = workingData.prevStackNode->depth;
        break;
      case CapLogType::BLOCK_INNER_LINE:
        expectedSelfDepth = workingData.prevStackNode->depth - 1;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        expectedSelfDepth = workingData.prevStackNode->depth - 1;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockScopeClose can't determine prev logline type");
    }
  } else {
    expectedSelfDepth = 1;
  }

  if ((selfDepth != expectedSelfDepth) || (expectedSelfDepth < 1)) {
    // TODO: use fix up method here later.
    std::cerr << "selfDepth: " << selfDepth << std::endl;
    std::cerr << "expectedSelfDepth: " << expectedSelfDepth << std::endl;
    failWithAbort(workingData, "selfDepth != expectedSelfDepth || expectedSelfDepth < 1");
  }

  const StackNode* callerStackNode = nullptr;
  if (workingData.prevStackNode) {
    switch (workingData.prevStackNode->logLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        // intentional fall-through
      case CapLogType::BLOCK_INNER_LINE:
        callerStackNode = workingData.prevStackNode->caller;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        if (workingData.prevStackNode->caller == nullptr) {
          failWithAbort(workingData, "workingData.prevStackNode->caller == nullptr");
        }
        callerStackNode = workingData.prevStackNode->caller->caller;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockScopeClose can't determine prev logline type");
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
    failWithAbort(workingData, "processBlockScopeClose Unabled to match infoStringBlockMatch with expected block opening line");
  }

  worldState.pushNewStackNode(std::move(outputLogData), callerStackNode);
}

void processBlockInnerLine (
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
        expectedSelfDepth = workingData.prevStackNode->depth + 1;
        break;
      case CapLogType::BLOCK_INNER_LINE:
        expectedSelfDepth = workingData.prevStackNode->depth;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        expectedSelfDepth = workingData.prevStackNode->depth;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockInnerLine can't determine prev logline type");
    }
  } else {
    expectedSelfDepth = 1;
  }

  if ((selfDepth != expectedSelfDepth) || (expectedSelfDepth < 1)) {
    // TODO: use fix up method here later.
    std::cout << "selfDepth: " << selfDepth << std::endl;
    std::cout << "expectedSelfDepth: " << expectedSelfDepth << std::endl;
    failWithAbort(workingData, "selfDepth != expectedSelfDepth || expectedSelfDepth < 1");
  }

  const StackNode* callerStackNode = nullptr;
  if (workingData.prevStackNode) {
    switch (workingData.prevStackNode->logLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        // intentional fall-through
      case CapLogType::BLOCK_INNER_LINE:
        callerStackNode = workingData.prevStackNode->caller;
        break;
      case CapLogType::BLOCK_SCOPE_CLOSE:
        if (workingData.prevStackNode->caller == nullptr) {
          failWithAbort(workingData, "workingData.prevStackNode->caller == nullptr");
        }
        callerStackNode = workingData.prevStackNode->caller->caller;
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "processBlockInnerLine can't determine prev logline type");
    }
  } else {
    callerStackNode = nullptr;
  }

  std::smatch pieces_match;
  bool matched = std::regex_match(workingData.inputLogLine->inputInfoString, pieces_match, infoStringInnerMatch);
  if(matched) {
    // TODO: Should move this into a similar "input" struct like the initial log line.
    // outputLogData.messageText.innerType = pieces_match[1];
    outputLogData.messageText.innerTypeString = pieces_match[1];
    outputLogData.messageText.innerPayload = pieces_match[2];
  } else {
    failWithAbort(workingData, "processBlockInnerLine Unabled to match infoStringBlockMatch with expected block opening line");
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
        processBlockScopeClose(workingData, worldState);
        break;
      case CapLogType::BLOCK_INNER_LINE:
        processBlockInnerLine(workingData, worldState);
        break;
      case CapLogType::UNKNOWN:
        failWithAbort(workingData, "Unknown input log line type");
    }
  }
  return matched;
}

bool processChannelLine(
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  std::smatch pieces_match;
  bool matched = std::regex_match(workingData.inputLine, pieces_match, channelLineRegex);
  if (matched) {
    workingData.lineType = CapLineType::CHANNEL;
    workingData.channelLine = std::make_unique<ChannelLine>();
    ChannelLine& channelLine = *workingData.channelLine.get();

    channelLine.fullString = pieces_match[0];
    channelLine.uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(pieces_match[1]);
    channelLine.uniqueThreadId = workingData.getUniqueThreadIdForInputThreadId(channelLine.uniqueProcessId, pieces_match[2]);
    channelLine.channelId = pieces_match[3];
    channelLine.enabled = pieces_match[4];
    channelLine.verbosityLevel = pieces_match[5];
    channelLine.channelName = pieces_match[6];

    worldState.pushChannelLine(std::move(channelLine));
  }

  return matched;
}

// TODO handle broken lines
// void adjustToExpectedDepth(expected depth)

[[maybe_unused]] size_t getFileSize(char* filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  return file.tellg();
}

size_t countLines(char* filename) {
  std::ifstream file(filename); 
  return std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
}

struct FileReadProgress {
  size_t currentLine = 0;
  size_t totalLines;
  
  float lastPrintedProgressPercent = 0.f;

  FileReadProgress(size_t totalLines): totalLines(totalLines){
    std::cout << "PROGRESS: " << std::endl;
  }

  void incrementProgress() {
    ++currentLine;
    float currentPercent = ((float)currentLine / totalLines) * 100.f;
    
    // print out in 1% increments
    if(currentPercent - lastPrintedProgressPercent >= 1.f) {
      std::cout << currentPercent << std::endl;
      lastPrintedProgressPercent = currentPercent;
    }
  }
};

} // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cout << "Usage: processClog [input clogfile.clog] [output file]";
    return 0;
  }
  char* inputFilename = argv[1];
  char* outputFilename = argv[2];

  // size_t inputFileSize = getFileSize(inputFilename);
  size_t inputFileLineCount = countLines(inputFilename);

  std::ifstream fileStream(inputFilename);

  OutputState output;
  // output.outputText.reserve(inputFileSize * 2);
  output.outputFileStream.open(outputFilename);

  WorldState worldState;
  WorldStateWorkingData worldWorkingData;

  FileReadProgress progress(inputFileLineCount);

  std::string inputLine;
  while (std::getline(fileStream, inputLine)) {
    worldWorkingData.inputLine = std::move(inputLine);
    if (processLogLine(worldWorkingData, worldState)) {
      // output.outputText.append 
    } else if (processChannelLine(worldWorkingData, worldState)) {
      // 
    }

    ++worldWorkingData.intputFileLineNumber;
    progress.incrementProgress();
  }

  std::cout << "Finished processessing input file.  Writing to output now." << std::endl;

  output.outputFileStream << "************************************************************************" << std::endl << std::endl;
  output.outputFileStream << "CAPTAINS LOG PROCESSED - VERSION 1" << std::endl << std::endl;
  output.outputFileStream << "************************************************************************" << std::endl << std::endl;
  output.outputFileStream << "CHANNEL INFO" << std::endl << std::endl;
  for (auto&& channelLinePtr : worldState.getChannelArray() ) {
    auto& channelLine = *channelLinePtr.get();
    output.outputFileStream
      << "P=" << channelLine.uniqueProcessId
      << " T=" << channelLine.uniqueThreadId
      << " CHANNEL-ID=" << channelLine.channelId
      << " : ENABLED=" << channelLine.enabled
      << " : VERBOSITY=" << channelLine.verbosityLevel
      << " : " << channelLine.channelName
      << std::endl;
  }

  output.outputFileStream << std::endl;
  output.outputFileStream << "************************************************************************" << std::endl << std::endl;

  for (auto&& nodePtr : worldState.getNodeArray() ) {
    auto& node = *nodePtr.get();
    output.outputFileStream
      << "P=" << node.uniqueProcessId
      << " T=" << node.uniqueThreadId
      << " C=" << node.commonLogText.channelId
      << " " << node.commonLogText.indentation
      << " " << node.commonLogText.functionId
      << " " << node.commonLogText.sourceFileLine;
    
    switch (node.logLineType) {
      case CapLogType::BLOCK_SCOPE_OPEN:
        // intentional fall through
      case CapLogType::BLOCK_SCOPE_CLOSE:
        output.outputFileStream << "::[" 
          << node.blockText.filename << "]::["
          << node.blockText.functionName << "] "
          << node.blockText.objectId;
        break;
      case CapLogType::BLOCK_INNER_LINE:
        output.outputFileStream << " :"
          << node.messageText.innerTypeString
          << " " << node.messageText.innerPayload;
        break;
      case CapLogType::UNKNOWN:
        std::cerr << "CapLogType::UNKNOWN line" << std::endl;
        std::abort();
    }

    output.outputFileStream << std::endl;
  }

  //std::cout << outputText << std::endl;

  return 0;
}

