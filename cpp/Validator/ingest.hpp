#pragma once

#include <iostream>
#include <iterator>
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
#include <string>

// #include <CaptainsLog/caplogger.hpp>

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
5 - CAP_PRIMARY_LOG_BEGIN_DELIMITER (start of block)
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

struct Pattern {
  std::string_view string;
  bool isNothingBeforeFirst = false;
  bool isOptional = false;

  Pattern(std::string_view string, bool isNothingBeforeFirst = false, bool isOptional = false) : string(string), isNothingBeforeFirst(isNothingBeforeFirst), isOptional(isOptional) {}
};

struct StringExtractor {
  // number of captures will be number of (patterns * 2) + 1
  // eg: for a pattern like {PatternToken{"aa"}, PatternToken{"bb"}, PatternToken{"cc"}}
  // the captures will be like 
  // <capture before><capture "aa"><capture between><capture "bb"><capture between><capture "cc"><capture end>
  StringExtractor(std::vector<Pattern> matchPatterns) : matchPatterns(matchPatterns) {
    captures.reserve((matchPatterns.size() * 2) + 1);
  }

  bool match(std::string_view inputString) {
    captures.clear();
    
    size_t lastStart = 0;
    size_t lastEnd = 0;
    int failedOptionalMatchCount = 0;
    for(const auto& pattern : matchPatterns) {
      if (pattern.isNothingBeforeFirst) {
        std::string_view exactMatchCheck = inputString.substr(lastStart, pattern.string.size());
        if (exactMatchCheck != pattern.string) {
          if (pattern.isOptional) {
            // although optional, it's first, so there's no string before this.
            captures.push_back("");
            captures.push_back("");
            return true;
          } else {
            return false;
          }
        }
        captures.push_back("");
        std::string temp1 = std::string(exactMatchCheck);
        captures.push_back(exactMatchCheck);
        lastEnd = lastStart + pattern.string.size();
      } else {
        size_t patternStart = inputString.find(pattern.string, lastEnd);
        if (patternStart == std::string::npos) {
          if (pattern.isOptional) {
            ++failedOptionalMatchCount;
            continue;
          } else {
            return false;
          }
        }
        
        // each match matches the fixed pattern followed by a variable string.  The variable
        // string ends when the next match starts.  Optional matches 
        lastEnd = patternStart;
        std::string temp1 = std::string(inputString.substr(lastStart, lastEnd - lastStart));
        captures.push_back(inputString.substr(lastStart, lastEnd - lastStart));

        // padding for failed optional parts.  For failed matches, the fixed string & the variable string
        // are both empty.
        for(int extraCount = 0; extraCount < failedOptionalMatchCount; ++extraCount) {
          captures.push_back("");
          captures.push_back("");
        }
        failedOptionalMatchCount = 0;

        lastStart = patternStart;
        lastEnd = lastStart + pattern.string.size();
        std::string temp2 = std::string(inputString.substr(lastStart, lastEnd - lastStart));
        captures.push_back(inputString.substr(lastStart, lastEnd - lastStart));
      }
      lastStart = lastEnd;
    }

    std::string temp3 = std::string(inputString.substr(lastEnd, inputString.size() - lastEnd));
    captures.push_back(inputString.substr(lastEnd, inputString.size() - lastEnd));

    // if the tail of the patterns are all optionals, this loop covers failed optionals at the end of the string.
    for(int extraCount = 0; extraCount < failedOptionalMatchCount; ++extraCount) {
      captures.push_back(""); // eg. this will be for "\n"
      captures.push_back(""); // eg. this is for the ignore at the end.
    }

    assert(captures.size() == (matchPatterns.size() * 2) + 1);

    return true;
  }

  std::vector<Pattern> matchPatterns;
  std::vector<std::string_view> captures;
};

struct CapLogMatcher {
  /**
  * This will match all the caplog logs
  * 0 - (ignore)
  * 1 - "CAP_LOG : "
  * 2 - (actual important caplog data)
  * 3 - "\n"
  * 4 - (ignore)
  * EG. "(.*)(CAP_LOG : )(.*)"
  **/
  StringExtractor caplog{{Pattern{"CAP_LOG : "}, Pattern{"\n", false, true}}};

  /**
  * DEPENDS ON caplog.captures[2]
  * This will match all the lines that start with process id
  * 0 - before (empty)
  * 1 - "P="
  * 2 - between (the process id number)
  * 3 - " "
  * 4 - after
  * EG. "()(P=)(.*)( )(.+?)"
  **/
  StringExtractor processId{{Pattern{"P=", true}, {Pattern{" "}}}};

  /**
  * DEPENDS ON processId.captures[4]
  * This will match the character limit line
  * 0 - (empty)
  * 1 - "MAX-CHAR-SIZE="
  * 2 - (the size)
  * EG. "()(MAX-CHAR-SIZE=)(.+?)",
  **/
  StringExtractor maxCharsLine{{Pattern{"MAX-CHAR-SIZE=", true}}};

  /**
  * DEPENDS ON processId.captures[4]
  * This will match all the lines that start with thread id
  * 0 - (empty)
  * 1 - "T="
  * 2 - (the thread id number)
  * 3 - " "
  * 4 - (tail)
  * EG. "()(T=)(.*)( )(.+?)"
  **/
  StringExtractor threadId{{Pattern{"T=", true}, {Pattern{" "}}}};

  /**
  * DEPENDS ON threadId.captures[4]
  * This will match all the channel logs
  * 0 - (empty)
  * 1 - "CHANNEL-ID="
  * 2 - (the channel ID number)
  * 3 - " : "
  * 4 - (enablement string)
  * 5 - " : VERBOSITY="
  * 6 - (VerbosityLevel)
  * 7 - " : "
  * 8 - (ChannelName)
  * EG(match): "()(CHANNEL-ID=)(.+?)( : )(.+?)( : VERBOSITY=)(.+?)( : )(.+?)"
  * EG(full):  CAP_LOG : P=2211240112 T=0 CHANNEL-ID=000 : FULLY ENABLED         : VERBOSITY=0 : >  DEFAULT
  **/
  StringExtractor channelLine{{
    Pattern{"CHANNEL-ID=", true},
    Pattern{" : "},
    Pattern{" : VERBOSITY="},
    Pattern{" : "}
  }};


  /**
  * DEPENDS ON threadId.captures[4]
  * This will match all the logs:
  * CAP_LOG_BLOCK, CAP_LOG_BLOCK_NO_THIS, CAP_LOG, CAP_LOG_ERROR, CAP_SET
  * the sub-expressions:
  * 0 - (empty)
  * 1 - "C="
  * 2 - (channel id)
  * 3 - " "
  * 4 - (lexical indentation symbols)
  * 5 - " "
  * 6 - (Info String (everything after the prefix and Indentation marker)
  * EG(match): "()(C=)(.+?)( )(.+?)( )(.*)"
  **/
  StringExtractor logLine{{
    Pattern{"C=", true},
    Pattern{" "},
    Pattern{" "}
  }};

  /**
  * DEPENDS ON logLine.captures[6]
  * 0 - (FunctionId)
  * 1 - " ["
  * 2 - (Line number in source code)
  * 3 - "]"
  * 4 - (Info string body; the remainder of the string.  Changes depending on log type.)
  * EG(match): "(.+?)( \\[)(.+?)(])(.*)"
  **/  
  StringExtractor infoStringCommon{{
    Pattern{" ["},
    Pattern{"]"},
  }};

  /**
  * DEPENDS ON infoStringCommon.captures[4]
  * This will match the info string body if it's opening and closing block tags
  * eg. ::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
  * 0 - (empty)
  * 1 - "::["
  * 2 - (filename)
  * 3 - "]::["
  * 4 - (Function name - may be truncated if too long)
  * 5 - "] "
  * 6 - (The object id.  In c++ this is the "this" pointer - or 0 if none)
  * EG: "(::\\[)(.*)(\\]::\\[)(.*)(\\] )([0-9a-z]+)",
  **/
  StringExtractor infoStringBlock{{
    Pattern{"::["},
    Pattern{"]::["},
    Pattern{"] "},
  }};

  /**
  * DEPENDS ON infoStringCommon.captures[4]
  * This will match the opening and closing block tags
  * eg. " LOG: audio graph END" 
  * 0 - (empty)
  * 1 - " "
  * 2 - (inner type: LOG/ERROR/SET)
  * 3 - ":"
  * 4 - (inner message)
  * EG: " (.*?):(.*)"
  **/
  StringExtractor infoStringInner{{
    Pattern{" "},
    Pattern{":"},
  }};
};

enum class CapLineType {
  CAPLOG,
  CHANNEL,
  MAXCHARLINE,
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
  std::string incompleteSpacePadding;
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
    , incompleteSpacePadding(std::move(logData.incompleteSpacePadding))
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
  std::string incompleteSpacePadding;

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
  // tracks what we've read so far in the file (files start counting from 1)
  size_t intputFileLineNumber = 1;
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

  std::unordered_map<size_t, int> uniqueProcessIdToMaxCharLine;

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

    // std::cout << inputProcessId << ":" << retId << std::endl;
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
  // std::abort();
}

// TODO make ostream<< for stack node

CapLogType getLineType(std::string_view inputIndentation) {
  CapLogType retType = CapLogType::UNKNOWN;
  if (inputIndentation.find("F") != std::string::npos) {
    retType = CapLogType::BLOCK_SCOPE_OPEN;
  } else if (inputIndentation.find("L") != std::string::npos) {
    retType = CapLogType::BLOCK_SCOPE_CLOSE;
  } else if (inputIndentation.find(">") != std::string::npos) {
    retType = CapLogType::BLOCK_INNER_LINE;
  } else if (inputIndentation.find("|+") != std::string::npos) {
    retType = CapLogType::BLOCK_CONCAT_BEGIN;
  } else if (inputIndentation.find("+|") != std::string::npos) {
    retType = CapLogType::BLOCK_CONCAT_END;
  } else if (inputIndentation.find("+") != std::string::npos) {
    retType = CapLogType::BLOCK_CONCAT_CONTINUE;
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
  std::string outputIndentation;
  for(int i = 0; i < inputIndentation.size(); i++) {
    switch(inputIndentation[i]) {
      case ':': outputIndentation += "║"; break;
      case 'F': outputIndentation += "╔"; break;
      case 'L': outputIndentation += "╚"; break;
      case '-': outputIndentation += "╠"; break;
      case '>': outputIndentation += "╾"; break;
      default: outputIndentation.push_back(inputIndentation[i]); break;
    }
  }
  return outputIndentation;
}

void processIncompleteLineBegin (
    WorldStateWorkingData& workingData,
    WorldState& worldState) {
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();
  OutputLogData& outputLogData = *workingData.outputLogData.get();

  outputLogData.isComplete = false;
  outputLogData.incompleteText = inputLogLine.inputInfoString;

  int characterLimit = workingData.uniqueProcessIdToMaxCharLine[outputLogData.uniqueProcessId];

  std::string padding;
  for(int i = inputLogLine.inputFullString.size(); i < characterLimit; i++) {
    padding += " ";
  }
  outputLogData.incompleteSpacePadding += padding;
  worldState.addNewStackNode(std::move(outputLogData), workingData.prevStackNode, workingData.inPlace);
}

void processIncompleteLineContinue (
    WorldStateWorkingData& workingData,
    [[maybe_unused]] WorldState& worldState) {
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();
  OutputLogData& outputLogData = *workingData.outputLogData.get();

  workingData.prevStackNode->incompleteString += workingData.prevStackNode->incompleteSpacePadding;
  workingData.prevStackNode->incompleteString += inputLogLine.inputInfoString;

  int characterLimit = workingData.uniqueProcessIdToMaxCharLine[outputLogData.uniqueProcessId];

  std::string padding;
  for(int i = inputLogLine.inputFullString.size(); i < characterLimit; i++) {
    padding += " ";
  }
  workingData.prevStackNode->incompleteSpacePadding = padding;
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
    expectedSelfDepth = selfDepth;
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

  CapLogMatcher matcher;
  if (matcher.infoStringBlock.match(workingData.inputLogLine->inputInfoString)) {
    outputLogData.blockText.filename = matcher.infoStringBlock.captures[2];
    outputLogData.blockText.functionName = matcher.infoStringBlock.captures[4];
    outputLogData.blockText.objectId = matcher.infoStringBlock.captures[6];
  } else {
    failWithAbort(workingData, "Unable to match infoStringBlockMatch with expected block opening line");
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
    expectedSelfDepth = selfDepth;
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
    
  CapLogMatcher matcher;
  if (matcher.infoStringBlock.match(workingData.inputLogLine->inputInfoString)) {
    outputLogData.blockText.filename = matcher.infoStringBlock.captures[2];
    outputLogData.blockText.functionName = matcher.infoStringBlock.captures[4];
    outputLogData.blockText.objectId = matcher.infoStringBlock.captures[6];
  } else {
    failWithAbort(workingData, "processBlockScopeClose Unable to match infoStringBlockMatch with expected block opening line");
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
    expectedSelfDepth = selfDepth;
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

  CapLogMatcher matcher;
  if (matcher.infoStringInner.match(workingData.inputLogLine->inputInfoString)) {
    outputLogData.messageText.innerTypeString = matcher.infoStringInner.captures[2];
    outputLogData.messageText.innerPayload = matcher.infoStringInner.captures[4];
  } else {
    failWithAbort(workingData, "processBlockInnerLine Unable to match infoStringBlockMatch with expected block opening line");
  }

  worldState.addNewStackNode(std::move(outputLogData), callerStackNode, workingData.inPlace);
}

bool processLogLine(
    WorldStateWorkingData& workingData,
    WorldState& worldState) {
  CapLogMatcher matcher;
  if (!matcher.processId.match(workingData.inputLine)) {
    return false;
  }

  if (!matcher.threadId.match(matcher.processId.captures[4])) {
    return false;
  }
  
  if (!matcher.logLine.match(matcher.threadId.captures[4])) {
    return false;
  }

  workingData.lineType = CapLineType::CAPLOG;
  workingData.inputLogLine = std::make_unique<InputLogLine>();
  workingData.outputLogData = std::make_unique<OutputLogData>();
  InputLogLine& inputLogLine = *workingData.inputLogLine.get();
  OutputLogData& outputLogData = *workingData.outputLogData.get();

  inputLogLine.inputFullString = workingData.inputLine;
  inputLogLine.inputProcessId = matcher.processId.captures[2];
  inputLogLine.inputThreadId = matcher.threadId.captures[2];
  inputLogLine.inputChannelId = matcher.logLine.captures[2];
  inputLogLine.inputIndentation = matcher.logLine.captures[4];
  inputLogLine.inputInfoString = matcher.logLine.captures[6];

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
      if(matcher.caplog.match(workingData.prevStackNode->incompleteString)) {
        workingData.inputLine = matcher.caplog.captures[2];
      } else {
        workingData.inputLine = std::move(workingData.prevStackNode->incompleteString);
      }
      processLogLine(workingData, worldState);
      break;
    default:
      isCompleteLine = true;
  }

  if (isCompleteLine) {
    if (matcher.infoStringCommon.match(inputLogLine.inputInfoString)) {
      inputLogLine.inputFunctionId = matcher.infoStringCommon.captures[0];
      inputLogLine.inputSourceFileLine = matcher.infoStringCommon.captures[2];
      inputLogLine.inputInfoString = matcher.infoStringCommon.captures[4]; //overwrite infoString with common part removed
      inputLogLine.inputLineDepth = getLineDepth(inputLogLine.inputIndentation);

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

  return true;
}

bool processChannelLine(
    WorldStateWorkingData& workingData,
    WorldState& worldState) {
  CapLogMatcher matcher;
  if (!matcher.processId.match(workingData.inputLine)) {
    return false;
  }

  if (!matcher.threadId.match(matcher.processId.captures[4])) {
    return false;
  }
  
  if (!matcher.channelLine.match(matcher.threadId.captures[4])) {
    return false;
  }

  workingData.lineType = CapLineType::CHANNEL;
  workingData.channelLine = std::make_unique<ChannelLine>();
  ChannelLine& channelLine = *workingData.channelLine.get();

  channelLine.fullString = workingData.inputLine;
  channelLine.uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(std::string(matcher.processId.captures[2]), worldState);
  channelLine.uniqueThreadId = workingData.getUniqueThreadIdForInputThreadId(channelLine.uniqueProcessId, std::string(matcher.threadId.captures[2]), worldState);
  channelLine.channelId = matcher.channelLine.captures[2];
  channelLine.enabledMode = matcher.channelLine.captures[4];
  channelLine.verbosityLevel = matcher.channelLine.captures[6];
  channelLine.channelName = matcher.channelLine.captures[8];

  worldState.pushChannelLine(std::move(channelLine));

  return true;
}

bool processLogLineCharLimit(
    WorldStateWorkingData& workingData,
    WorldState& worldState) {
  CapLogMatcher matcher;
  if (!matcher.processId.match(workingData.inputLine)) {
    return false;
  }

  if (!matcher.maxCharsLine.match(matcher.processId.captures[4])) {
    return false;
  }

  size_t uniqueProcessId = workingData.getUniqueProcessIdForInputProcessId(std::string(matcher.processId.captures[4]), worldState);
  workingData.uniqueProcessIdToMaxCharLine[uniqueProcessId] = std::stoi(std::string(matcher.maxCharsLine.captures[2]));
  return true;
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

void ProcessCaplogLine(const std::string& inputString, WorldStateWorkingData& workingData, WorldState& worldState) {
  CapLogMatcher matcher;
  if(!matcher.caplog.match(inputString)) {
    return;
  }
  workingData.inputLine = matcher.caplog.captures[2];
  if (matcher.processId.match(workingData.inputLine)) {
    matcher.threadId.match(matcher.processId.captures[4]);
  }

  if (processLogLine(workingData, worldState)) {
    //
  } else if (processChannelLine(workingData, worldState)) {
    //
  } else if (processLogLineCharLimit(workingData, worldState)) {
    //
  }
  workingData.inPlace = std::nullopt;
  workingData.inputLine = "";
}