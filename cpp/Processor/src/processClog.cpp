#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <fstream>
#include <unordered_map>
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <assert.h>
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
  ".*CAP_LOG : P=(.+?) T=(.+?) CHANNEL-ID=(.+?) : (.+?) : VERBOSITY=(.+?) : (.+?)",
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
 * 5 - Info String (everything after the prefix and Indentation marker)
 **/
std::regex logLineRegex(
  "(.*?)CAP_LOG : P=(.+?) T=(.+?) C=(.+?) (.+?) (.*)",
  std::regex_constants::ECMAScript);

/**
 * This will match the Info String (#5) from the logLineRegex
 * 1 - FunctionId
 * 2 - Line number in source code
 * 3 - Info string body; the remainder of the string.  Changes depending on log type.
 **/
std::regex infoStringCommon(
  "(.+?) (\\[.+?\\])(.*)",
  std::regex_constants::ECMAScript);

/**
 * This will match the info string body if it's opening and closing block tags
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
  BLOCK_CONCAT_BEGIN,
  BLOCK_CONCAT_CONTINUE,
  BLOCK_CONCAT_END,
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

  // incomplete string
  bool isComplete = true;
  std::string incompleteText;
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
  std::string enabledMode;
  std::string verbosityLevel;
  std::string channelName;
};

// should rename this, this only represents CapLog types, not channels.
struct StackNode {
  StackNode(
    int line, 
    OutputLogData&& logData,
    StackNode* caller)
    : line(line)
    , depth(std::move(logData.lineDepth))
    , uniqueProcessId(std::move(logData.uniqueProcessId))
    , uniqueThreadId(std::move(logData.uniqueThreadId))
    , commonLogText(std::move(logData.commonLogText))
    , logLineType(std::move(logData.logLineType))
    , blockText(std::move(logData.blockText))
    , messageText(std::move(logData.messageText))
    , caller(caller)
    , isComplete(logData.isComplete)
    , incompleteString(std::move(logData.incompleteText))
    , loggedObject(nullptr) {}

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

  StackNode* caller;
  
  bool isComplete;

  std::string incompleteString;

  LoggedObject* loggedObject;
};

class WorldState {
public:
  using StackNodeArray = std::vector<std::unique_ptr<StackNode>>;
  using ChannelArray = std::vector<std::unique_ptr<ChannelLine>>;
  using UniqueProcessIdToChannelArray = std::map<size_t, ChannelArray>;

  struct InPlace {
    size_t index;
    StackNode* stackNode;
  };

  // should use expected, but that's only in c++23
  // the objectId is address of the object for logs in c++
  enum ExistsPolicy{DontCreateIfNotExist, CreateIfNotExist};
  LoggedObject* getLoggedObject(const std::string& objectId, ExistsPolicy existsPolicy) {
    if (objectId.empty()) {
      return nullptr;
    }

    LoggedObject* retLoggedObject = nullptr;
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

  StackNode& addNewStackNode(
      OutputLogData&& logData,
      StackNode* caller,
      std::optional<InPlace> inPlace) {
    if (!inPlace) {
      size_t stackNodeIdx = mStackNodeArray.size();
      mStackNodeArray.emplace_back(std::make_unique<StackNode>(
        stackNodeIdx, std::move(logData), caller));
      mProcessToThreadToStackNodeIds[logData.uniqueProcessId][logData.uniqueThreadId].emplace_back(stackNodeIdx);
      return *mStackNodeArray.back().get();
    } else {
      size_t stackNodeIdx = inPlace.value().index;
      mStackNodeArray[stackNodeIdx] = std::make_unique<StackNode>(
        stackNodeIdx, std::move(logData), caller);
      return *mStackNodeArray[stackNodeIdx].get();
    }
  }

  StackNode* getStackNodeOnLine(size_t lineNumber) {
    if (lineNumber < mStackNodeArray.size()) {
      return mStackNodeArray[lineNumber].get();
    } else {
      return nullptr;
    }
  }

  std::tuple<size_t, StackNode*> getLastStackNodeForProcessThread(size_t uniqueProcessId, size_t uniqueThreadId) {
    assert(mProcessToThreadToStackNodeIds.size() > uniqueProcessId);
    assert(mProcessToThreadToStackNodeIds[uniqueProcessId].size() > uniqueThreadId);
    if (!mProcessToThreadToStackNodeIds[uniqueProcessId][uniqueThreadId].empty()) {
      size_t stackNodeIdx = mProcessToThreadToStackNodeIds[uniqueProcessId][uniqueThreadId].back();
      return std::tuple<size_t, StackNode*>(stackNodeIdx, mStackNodeArray[stackNodeIdx].get());
    }
    return std::tuple<size_t, StackNode*>(0, nullptr);
  }

  const StackNodeArray& getNodeArray() const {
    return mStackNodeArray;
  }

  ChannelLine& pushChannelLine(ChannelLine&& channelLine) {
    mUniqueProcessIdToChannelArray[channelLine.uniqueProcessId].emplace_back(std::make_unique<ChannelLine>(std::move(channelLine)));
    return *mUniqueProcessIdToChannelArray[channelLine.uniqueProcessId].back().get();
  }

  const UniqueProcessIdToChannelArray& getChannelArrayMap() const {
    return mUniqueProcessIdToChannelArray;
  }

  size_t newUniqueProcessId() {
    auto retVal = mProcessToThreadToStackNodeIds.size();
    mProcessToThreadToStackNodeIds.emplace_back();
    return retVal;
  }

  size_t newUniqueThreadId(size_t uniqueProcessId) {
    auto retVal = mProcessToThreadToStackNodeIds[uniqueProcessId].size();
    mProcessToThreadToStackNodeIds[uniqueProcessId].emplace_back();
    return retVal;
  }

private:
  std::unordered_map<std::string, std::unique_ptr<LoggedObject>> mLoggedObjects;

  UniqueProcessIdToChannelArray mUniqueProcessIdToChannelArray;

  StackNodeArray mStackNodeArray;

  using IdxArray = std::vector<size_t>;
  using ProcessToThreadToStackNodeIds = std::vector<std::vector<IdxArray>>;
  ProcessToThreadToStackNodeIds mProcessToThreadToStackNodeIds;
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

  std::optional<WorldState::InPlace> inPlace;

  size_t getUniqueProcessIdForInputProcessId(const std::string& inputProcessId, WorldState& world) {
    size_t retId;
    if (auto findUniqueProcessIdIter = mProcessToUniqueProcessId.find(inputProcessId); 
        findUniqueProcessIdIter != mProcessToUniqueProcessId.end()) {
      retId = findUniqueProcessIdIter->second;
    } else {
      retId = world.newUniqueProcessId();
      mProcessToUniqueProcessId[inputProcessId] = retId;
      mUniqueProcessIdToInputThreadToUniqueThreadId[retId];
    }

    // std::cout << retId << std::endl;
    return retId;
  }

  size_t getUniqueThreadIdForInputThreadId(size_t uniqueProcessId, const std::string& inputThreadId, WorldState& world) {
    size_t retId;
    if (auto findThreadMapIter = mUniqueProcessIdToInputThreadToUniqueThreadId.find(uniqueProcessId);
        findThreadMapIter != mUniqueProcessIdToInputThreadToUniqueThreadId.end()) {
      if (auto findUniqueThreadIdIter = findThreadMapIter->second.find(inputThreadId); 
          findUniqueThreadIdIter != findThreadMapIter->second.end()) {
        retId = findUniqueThreadIdIter->second;
      } else {
        retId = world.newUniqueThreadId(uniqueProcessId);
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
  std::unordered_map<std::string, size_t> mProcessToUniqueProcessId;
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

// TODO make ostream<< for stack node

CapLogType getLineType(const std::string& inputIndentation) {
  CapLogType retType = CapLogType::UNKNOWN;
  std::smatch match;
  if (std::regex_match(inputIndentation, match, std::regex(".*F"))) {
    retType = CapLogType::BLOCK_SCOPE_OPEN;
  } else if (std::regex_match (inputIndentation, match, std::regex(".*L"))) {
    retType = CapLogType::BLOCK_SCOPE_CLOSE;
  } else if (std::regex_match (inputIndentation, match, std::regex(".*>"))) {
    retType = CapLogType::BLOCK_INNER_LINE;
  } else if (std::regex_match (inputIndentation, match, std::regex("\\|\\+"))) {
    retType = CapLogType::BLOCK_CONCAT_BEGIN;
  } else if (std::regex_match (inputIndentation, match, std::regex("\\+\\+"))) {
    retType = CapLogType::BLOCK_CONCAT_CONTINUE;
  } else if (std::regex_match (inputIndentation, match, std::regex("\\+\\|"))) {
    retType = CapLogType::BLOCK_CONCAT_END;
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

void processIncompleteLineBegin (
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();
  OutputLogData& outputLogData = *workingData.outputLogData.get();

  outputLogData.isComplete = false;
  outputLogData.incompleteText = inputLogLine.inputInfoString;
  worldState.addNewStackNode(std::move(outputLogData), workingData.prevStackNode, workingData.inPlace);
}

void processIncompleteLineContinue (
    WorldStateWorkingData& workingData, 
    [[maybe_unused]] WorldState& worldState) {
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();

  workingData.prevStackNode->incompleteString += inputLogLine.inputInfoString;
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
      case CapLogType::BLOCK_CONCAT_BEGIN:
      case CapLogType::BLOCK_CONCAT_CONTINUE:
      case CapLogType::BLOCK_CONCAT_END:
        // TODO: we can "recover" here because we should know the delimiter and therefore the "real"
        // type of the previous line.
        failWithAbort(workingData, "previous line is incomplete");
        break;
      default:
        failWithAbort(workingData, "processBlockScopeOpen can't determine prev logline type");
    }
  } else {
    expectedSelfDepth = 1;
  }

  if ((selfDepth != expectedSelfDepth) || (expectedSelfDepth < 1)) {
    // TODO: use fix up method here later; add extra lines as necessary for the depth.
    std::cerr << "selfDepth: " << selfDepth << std::endl;
    std::cerr << "expectedSelfDepth: " << expectedSelfDepth << std::endl;
    failWithAbort(workingData, "selfDepth != expectedSelfDepth || expectedSelfDepth < 1");
  }

  StackNode* callerStackNode = nullptr;
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
      default:
        failWithAbort(workingData, "processBlockScopeOpen can't determine prev logline type");
    }
  } else {
    callerStackNode = nullptr;
  }

  std::smatch piecesMatch;
  bool matched = std::regex_match(workingData.inputLogLine->inputInfoString, piecesMatch, infoStringBlockMatch);
  if(matched) {
    // TODO: Should move this into a similar "input" struct like the initial log line.
    outputLogData.blockText.filename = piecesMatch[1];
    outputLogData.blockText.functionName = piecesMatch[2];
    outputLogData.blockText.objectId = piecesMatch[3];
  } else {
    failWithAbort(workingData, "Unabled to match infoStringBlockMatch with expected block opening line");
  }

  worldState.addNewStackNode(std::move(outputLogData), callerStackNode, workingData.inPlace);
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
      case CapLogType::BLOCK_CONCAT_BEGIN:
      case CapLogType::BLOCK_CONCAT_CONTINUE:
      case CapLogType::BLOCK_CONCAT_END:
        // TODO: we can "recover" here because we should know the delimiter and therefore the "real"
        // type of the previous line.
        failWithAbort(workingData, "previous line is incomplete");
        break;
      default:
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

  StackNode* callerStackNode = nullptr;
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
      default:
        failWithAbort(workingData, "processBlockScopeClose can't determine prev logline type");
    }
  } else {
    callerStackNode = nullptr;
  }

  std::smatch piecesMatch;
  bool matched = std::regex_match(workingData.inputLogLine->inputInfoString, piecesMatch, infoStringBlockMatch);
  if(matched) {
    // TODO: Should move this into a similar "input" struct like the initial log line.
    outputLogData.blockText.filename = piecesMatch[1];
    outputLogData.blockText.functionName = piecesMatch[2];
    outputLogData.blockText.objectId = piecesMatch[3];
  } else {
    failWithAbort(workingData, "processBlockScopeClose Unabled to match infoStringBlockMatch with expected block opening line");
  }

  worldState.addNewStackNode(std::move(outputLogData), callerStackNode, workingData.inPlace);
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
      case CapLogType::BLOCK_CONCAT_BEGIN:
      case CapLogType::BLOCK_CONCAT_CONTINUE:
      case CapLogType::BLOCK_CONCAT_END:
        // TODO: we can "recover" here because we should know the delimiter and therefore the "real"
        // type of the previous line.
        failWithAbort(workingData, "previous line is incomplete");
        break;
      default:
        failWithAbort(workingData, "processBlockInnerLine can't determine prev logline type");
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

  StackNode* callerStackNode = nullptr;
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
      default:
        failWithAbort(workingData, "processBlockInnerLine can't determine prev logline type");
    }
  } else {
    callerStackNode = nullptr;
  }

  std::smatch piecesMatch;
  bool matched = std::regex_match(workingData.inputLogLine->inputInfoString, piecesMatch, infoStringInnerMatch);
  if(matched) {
    // TODO: Should move this into a similar "input" struct like the initial log line.
    // outputLogData.messageText.innerType = piecesMatch[1];
    outputLogData.messageText.innerTypeString = piecesMatch[1];
    outputLogData.messageText.innerPayload = piecesMatch[2];
  } else {
    failWithAbort(workingData, "processBlockInnerLine Unabled to match infoStringBlockMatch with expected block opening line");
  }

  worldState.addNewStackNode(std::move(outputLogData), callerStackNode, workingData.inPlace);
}

bool processLogLine(
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  std::smatch piecesMatch;
  bool matched = std::regex_match (workingData.inputLine, piecesMatch, logLineRegex);
  if (matched) {
    workingData.lineType = CapLineType::CAPLOG;
    workingData.inputLogLine = std::make_unique<InputLogLine>();
    workingData.outputLogData = std::make_unique<OutputLogData>();
    InputLogLine& inputLogLine = *workingData.inputLogLine.get();
    OutputLogData& outputLogData = *workingData.outputLogData.get();

    // structured bindings don't work for regex match :(
    // cannot decompose inaccessible member ‘std::__cxx11::match_results<__gnu_cxx::__normal_iterator<...
    inputLogLine.inputFullString = piecesMatch[0];
    inputLogLine.inputProcessId = piecesMatch[2];
    inputLogLine.inputThreadId = piecesMatch[3];
    inputLogLine.inputChannelId = piecesMatch[4];
    inputLogLine.inputIndentation = piecesMatch[5];
    inputLogLine.inputInfoString = piecesMatch[6];

    // std::cout << "processLogLine start" << std::endl;
    // std::cout << piecesMatch[0] << std::endl;
    // std::cout << piecesMatch[1] << std::endl;
    // std::cout << piecesMatch[2] << std::endl;
    // std::cout << piecesMatch[3] << std::endl;
    // std::cout << piecesMatch[4] << std::endl;
    // std::cout << piecesMatch[5] << std::endl;
    // std::cout << piecesMatch[6] << std::endl;
    // std::cout << "processLogLine end" << std::endl;

    inputLogLine.inputLineType = getLineType(inputLogLine.inputIndentation);
    outputLogData.uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(inputLogLine.inputProcessId, worldState);
    outputLogData.uniqueThreadId = workingData.getUniqueThreadIdForInputThreadId(outputLogData.uniqueProcessId, inputLogLine.inputThreadId, worldState);
    outputLogData.logLineType = inputLogLine.inputLineType;

    // inputLogLine.inputLineDepth can resolve to a block open/close or inner log/error/set
    auto&& [prevStackNodeIdx, prevStackNode] = worldState.getLastStackNodeForProcessThread(outputLogData.uniqueProcessId,
                                                                                          outputLogData.uniqueThreadId);
    workingData.prevStackNode = prevStackNode;

    if (workingData.inPlace) {
      if (workingData.inPlace.value().stackNode != prevStackNode) {
        failWithAbort(workingData, "AFTER INCOMPLETE LINE: workingData.inPlace.stackNode != prevStackNode");
      } else if (workingData.inPlace.value().index != prevStackNodeIdx) {
        failWithAbort(workingData, "AFTER INCOMPLETE LINE: workingData.inPlace.index != prevStackNodeIdx");
      } else if (workingData.inPlace.value().stackNode->uniqueProcessId != outputLogData.uniqueProcessId) {
        failWithAbort(workingData, "AFTER INCOMPLETE LINE: workingData.inPlace.value().stackNode->uniqueProcessId != outputLogData.uniqueProcessId");
      } else if (workingData.inPlace.value().stackNode->uniqueThreadId != outputLogData.uniqueThreadId) {
        failWithAbort(workingData, "AFTER INCOMPLETE LINE: workingData.inPlace.value().stackNode->uniqueThreadId != outputLogData.uniqueThreadId");
      }

      // since the "last stack node" in this case is the inplace one we're modifying, we need to set the previous stack node to it's previous.
      workingData.prevStackNode = workingData.inPlace.value().stackNode->caller;
    }

    bool isCompleteLine = false;
    switch (inputLogLine.inputLineType) {
      case CapLogType::BLOCK_CONCAT_BEGIN:
        processIncompleteLineBegin(workingData, worldState);
        break;
      case CapLogType::BLOCK_CONCAT_CONTINUE:
        if (!prevStackNode) {
          failWithAbort(workingData, "Cannot concat; no previous node to concat to");
        }
        processIncompleteLineContinue(workingData, worldState);
        break;
      case CapLogType::BLOCK_CONCAT_END:
        workingData.inPlace = {prevStackNodeIdx, prevStackNode};
        workingData.inputLine = std::move(workingData.prevStackNode->incompleteString);
        processLogLine(workingData, worldState);
        break;
      default:
        isCompleteLine = true;  
    }

    if (isCompleteLine) {
      std::smatch infoMatch;
      bool matchedInfo = std::regex_match (inputLogLine.inputInfoString, infoMatch, infoStringCommon);
      
      // do common part of Info line.
      if (matchedInfo) {
        inputLogLine.inputFunctionId = infoMatch[1];
        inputLogLine.inputSourceFileLine = infoMatch[2];
        inputLogLine.inputInfoString = infoMatch[3]; //overwrite infoString with common part removed
        inputLogLine.inputLineDepth = getLineDepth(inputLogLine.inputIndentation);

        // std::cout << "TEST" << std::endl;
        // std::cout << inputLogLine.inputFunctionId << std::endl;
        // std::cout << inputLogLine.inputSourceFileLine << std::endl;
        // std::cout << inputLogLine.inputInfoString << std::endl;
        // std::cout << inputLogLine.inputLineDepth << std::endl;
        // std::cout << inputLogLine.inputIndentation << std::endl;
        // std::cout << (int)inputLogLine.inputLineType << std::endl;
        // std::cout << "END" << std::endl;

        outputLogData.lineDepth = inputLogLine.inputLineDepth;
        outputLogData.commonLogText.channelId = inputLogLine.inputChannelId;
        outputLogData.commonLogText.indentation = replaceIndentationChars(inputLogLine.inputIndentation);
        outputLogData.commonLogText.functionId = inputLogLine.inputFunctionId;
        outputLogData.commonLogText.sourceFileLine = inputLogLine.inputSourceFileLine;          

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
          default:
            failWithAbort(workingData, "Unknown input log line type");
        }
      } else {
        failWithAbort(workingData, "Unable to match info line common");
      }
    }
  }

  workingData.inPlace = std::nullopt;
  workingData.inputLine = "";

  return matched;
}

bool processChannelLine(
    WorldStateWorkingData& workingData, 
    WorldState& worldState) {
  std::smatch piecesMatch;
  bool matched = std::regex_match(workingData.inputLine, piecesMatch, channelLineRegex);
  if (matched) {
    workingData.lineType = CapLineType::CHANNEL;
    workingData.channelLine = std::make_unique<ChannelLine>();
    ChannelLine& channelLine = *workingData.channelLine.get();

    channelLine.fullString = piecesMatch[0];
    channelLine.uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(piecesMatch[1], worldState);
    channelLine.uniqueThreadId = workingData.getUniqueThreadIdForInputThreadId(channelLine.uniqueProcessId, piecesMatch[2], worldState);
    channelLine.channelId = piecesMatch[3];
    channelLine.enabledMode = piecesMatch[4];
    channelLine.verbosityLevel = piecesMatch[5];
    channelLine.channelName = piecesMatch[6];

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
  for (auto&& channelMapLine : worldState.getChannelArrayMap() ) {
    output.outputFileStream << "CHANNEL INFO FOR PROCESS ID " << channelMapLine.first << std::endl;
    for(auto&& channelLinePtr : channelMapLine.second) {
      auto& channelLine = *channelLinePtr.get();
      output.outputFileStream
        << "P=" << channelLine.uniqueProcessId
        << " T=" << channelLine.uniqueThreadId
        << " CHANNEL-ID=" << channelLine.channelId
        << " : " << channelLine.enabledMode
        << " : VERBOSITY=" << channelLine.verbosityLevel
        << " : " << channelLine.channelName
        << std::endl;
    }
    output.outputFileStream << std::endl;
  }

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
        output.outputFileStream << " "
          << node.messageText.innerTypeString
          << ": " << node.messageText.innerPayload;
        break;
      default:
        std::cerr << "CapLogType::UNKNOWN line" << std::endl;
        std::abort();
    }

    output.outputFileStream << std::endl;
  }

  std::cout << "Complete" << std::endl;

  return 0;
}

