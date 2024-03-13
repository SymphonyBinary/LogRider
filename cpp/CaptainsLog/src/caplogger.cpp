#ifdef ENABLE_CAP_LOGGER

#include "caplogger.hpp"
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>
#include <assert.h>

namespace CAP {


static const char* colourArray[] = {
  COLOUR BOLD CAP_RED,
  COLOUR BOLD CAP_GREEN,
  COLOUR BOLD CAP_YELLOW,
  COLOUR BOLD CAP_BLUE,
  COLOUR BOLD CAP_MAGENTA,
  COLOUR BOLD CAP_CYAN,
  COLOUR BOLD CAP_WHITE,
};
static const int colourArraySize = sizeof(colourArray)/sizeof(colourArray[0]);

//Before, I was using thread_local to store the per-thread state so that I wouldn't have to deal with mutexes or anything
//somehow on Android, thread_local didn't really do the expected things.  Also the process is

namespace {
void printChannel(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelId, std::string_view channelName, uint32_t enabledMode, int verbosityLevel) {
  ss << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
    << processId << " " << THREAD_ID_DELIMITER << colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD CAP_GREEN 
    << " CHANNEL-ID=" << std::setw(3) << std::setfill('0') << channelId;

  if (enabledMode == FULLY_ENABLED) {
    ss << " : FULLY ENABLED        ";
  } else if (enabledMode == ENABLED_NO_OUTPUT) {
    ss << " : ENABLED BUT NO OUTPUT";
  } else if (enabledMode == FULLY_DISABLED) {
    ss << " : FULLY DISABLED       ";
  } else {
    ss << " : UNKNOWN MODE!        ";
  }

  ss << " : VERBOSITY=" << verbosityLevel << " : ";

  for(unsigned int i = 0 ; i < depth; ++i ) {
    ss << ">  ";
  }

  ss << channelName;
}

void printTab(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelId) {
  ss << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
    << processId << " " << THREAD_ID_DELIMITER << colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD CAP_GREEN 
    << " " << CHANNEL_ID_DELIMITER << std::setw(3) << std::setfill('0') << channelId << " ";

  for(unsigned int i = 0 ; i < depth; ++i ){
    ss << TAB_DELIMITER;
  }
}

struct PrintPrefix {
  unsigned int processId;
  unsigned int threadId;
  unsigned int channelId;
};

// ss << << message
// writeOutput(ss)
// (PrintPrefix + TabDelims{depth}  << ss.str().substring(max - prefix))

std::ostream& operator<<(std::ostream& os, const PrintPrefix& printPrefix) {
  os << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
    << printPrefix.processId << " " << THREAD_ID_DELIMITER << colourArray[printPrefix.threadId % colourArraySize] << printPrefix.threadId << COLOUR BOLD CAP_GREEN 
    << " " << CHANNEL_ID_DELIMITER << std::setw(3) << std::setfill('0') << printPrefix.channelId << " ";
  return os;
}

struct TabDelims{
  unsigned int depth;
};

std::ostream& operator<<(std::ostream& os, const TabDelims& tabDelims) {
  for(unsigned int i = 0 ; i < tabDelims.depth; ++i ){
    os << TAB_DELIMITER;
  }
  return os;
}

void writeOutput(const std::string& messageBuffer, unsigned int processId, unsigned int threadId, unsigned int channelId, unsigned int depth) {
  std::stringstream completeOutputStream;
  completeOutputStream << PrintPrefix{processId, threadId, channelId} << TabDelims{depth} << messageBuffer;
  std::string completeOutputString = completeOutputStream.str();

  PRINT_TO_LOG("%s", completeOutputString.substr(0, LOG_LINE_CHARACTER_LIMIT).c_str());
  size_t index = LOG_LINE_CHARACTER_LIMIT;

  if (index < completeOutputString.size()) {
    std::stringstream concatStream;
    concatStream << PrintPrefix{processId, threadId, channelId} << CONCAT_DELIMITER;
    const std::string concatPrefixString = concatStream.str();
    size_t concatPrefixLength = concatPrefixString.size();
    assert(concatPrefixLength < LOG_LINE_CHARACTER_LIMIT);
    const size_t substrMax = LOG_LINE_CHARACTER_LIMIT - concatPrefixLength;
    while(index < completeOutputString.size()) {
      std::string currentLine = concatPrefixString + completeOutputString.substr(index, substrMax);
      PRINT_TO_LOG("%s", currentLine.c_str());
      index += substrMax;
    }
  }
}
} // namespace


// void writeOutput(const std::string& messageBuffer, unsigned int processId, unsigned int threadId, unsigned int channelId, unsigned int depth) {
//   std::stringstream ss;
//   ss << PrintPrefix{processId, threadId, channelId} << CONCAT_DELIMITER;
//   const std::string prefixString = ss.str();
//   size_t prefixLength = prefixString.size();

//   PRINT_TO_LOG("%s", messageBuffer.substr(0, LOG_LINE_CHARACTER_LIMIT).c_str());
//   size_t index = LOG_LINE_CHARACTER_LIMIT;

//   const size_t substrMax = LOG_LINE_CHARACTER_LIMIT - prefixLength;
//   while(index < messageBuffer.size()) {
//     std::string currentLine = prefixString + messageBuffer.substr(index, substrMax);
//     PRINT_TO_LOG("%s", currentLine.c_str());
//     index += substrMax;
//   }
// }


/*static*/ BlockChannelTree& BlockChannelTree::getInstance() {
  static BlockChannelTree instance;
  return instance;
}

uint32_t BlockChannelTree::getEnabledMode(CHANNEL channel) {
  return mEnabledModeChannelsById[(size_t)channel];
}

BlockChannelTree::BlockChannelTree() {
  PRINT_TO_LOG("%s", "CAPTAIN'S LOG - VERSION 1.1"); \

  std::vector<uint32_t> enabledStack = {ALL_FLAGS};
  int index = 0;
  uint32_t currentEnabledMode = ALL_FLAGS;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabledMode) \
  currentEnabledMode = enabledMode & enabledStack.back(); \
  mEnabledModeChannelsById[index++] = currentEnabledMode;

  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  enabledStack.emplace_back(currentEnabledMode);

  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  enabledStack.pop_back();

  #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)

  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN


  // Now print it.  This should be the first thing caplog prints.
  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();
  int channelDepth = 0;
  int channelId = 0;
  std::stringstream ss;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, channelEnabledMode) \
  printChannel(ss, logData.processTimestamp, logData.relativeThreadIdx, channelDepth, channelId, #name, mEnabledModeChannelsById[channelId], verboseLevel); \
  ++channelId; \
  PRINT_TO_LOG("%s", ss.str().c_str()); \
  ss.str("");
  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  ++channelDepth;
  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  --channelDepth;

  #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
  
  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
}


BlockLogger::BlockLogger(const void* thisPointer, CAP::CHANNEL channel)
  : mEnabledMode(BlockChannelTree::getInstance().getEnabledMode(channel))
  , mlogInfoBuffer()
  , mcustomMessageBuffer()
  , mId(0)
  , mDepth(0)
  , mThreadId(0)
  , mProcessId(0)
  , mChannel(channel)
  , mThisPointer(thisPointer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    //If this block is silent and can't write to output, no need to record 
    //depth, id, etc which are used for printing to the log.
    return;
  } 

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();

  mDepth = logData.logDepth;
  mId = logData.perThreadUniqueFunctionIdx;
  mThreadId = logData.relativeThreadIdx;
  mProcessId = logData.processTimestamp;
}

void BlockLogger::setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  mlogInfoBuffer = std::move(logInfoBuffer.substr(0, LOG_INFO_BUFFER_LIMIT));
  if (mlogInfoBuffer.length() != logInfoBuffer.length()) {
    mlogInfoBuffer.append("...]");
  }
  mlogInfoBuffer.append(" ");

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_BEGIN_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());

  if(customMessageBuffer.size() == 1) {
    return;
  }

  log(line, std::move(customMessageBuffer));
}

void BlockLogger::log(int line, std::string_view messageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  std::stringstream ss;
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] LOG: " 
  << messageBuffer << COLOUR RESET;
  writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

  // size_t index = 0;
  // while(index < messageBuffer.size()) {
  //   // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
  //   std::stringstream ss;
  //   printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  //   ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  //   << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] LOG: " 
  //   << messageBuffer.substr(index, LOG_LINE_CHARACTER_LIMIT) << COLOUR RESET;
  //   PRINT_TO_LOG("%s", ss.str().c_str());
  //   index += LOG_LINE_CHARACTER_LIMIT;
  // }
}

void BlockLogger::error(int line, std::string_view messageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  size_t index = 0;
  while(index < messageBuffer.size()) {
    // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "ERROR: " 
    << messageBuffer.substr(index, LOG_LINE_CHARACTER_LIMIT) << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
    index += LOG_LINE_CHARACTER_LIMIT;
  }
}

//-------- State Addresses
void BlockLogger::printState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto states = loggerDataStore.getStates(storeKeyList(key), variableNames(varName));

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    printStateImpl(line, "PRINT STATE", to_string(key), varName, states[0]);
  }
}

void BlockLogger::printAllStateOfStore(int line, const DataStoreKey& key) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto allStates = loggerDataStore.getAllStates(key);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED 
  << "PRINTING ALL STATE IN STORE: StoreKey='" << to_string(key) << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());

  for(const auto& row : allStates) {
    printStateImpl(line, "PRINT STATE", to_string(key), row.first, row.second);
  }
}

void BlockLogger::releaseState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName) {
  if(!(mEnabledMode)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> oldState = loggerDataStore.releaseState(key, varName);

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    printStateImpl(line, "RELEASE STATE", to_string(key), varName, oldState);
  }
}

void BlockLogger::releaseAllStateOfStore(int line, const DataStoreKey& key) {
  if(!(mEnabledMode)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedCount = loggerDataStore.releaseAllStates(key);

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED 
    << "RELEASE ALL STATE IN STORE: StoreKey='" << to_string(key) << "' Num Deleted='" << deletedCount << "'" << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::printStateImpl(int line, const std::string& logCommand, const std::string& storeKey, const std::string& varName, const std::optional<std::string>& value) {
  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << logCommand << ": "
  << "StoreKey='" << storeKey << "' : StateName='" << varName << "' : Value='" << value.value_or("N/A") << "'" << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

BlockLogger::~BlockLogger() {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    // block logger instance is only created when logging/output mode enabled.
    return;
  }

  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

uint32_t BlockLogger::getEnabledMode() const {
  return mEnabledMode;
}

} // namespace CAP

#endif
