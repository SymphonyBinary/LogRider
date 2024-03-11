#ifdef ENABLE_CAP_LOGGER

#include "caplogger.hpp"
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>


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
} // namespace


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
  printChannel(ss, logData.processTimestamp, logData.relativeThreadIdx, channelDepth, channelId++, #name, channelEnabledMode, verboseLevel); \
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
  , mChannel(channel) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();

  mDepth = logData.logDepth;
  mId = logData.perThreadUniqueFunctionIdx;
  mThreadId = logData.relativeThreadIdx;
  mProcessId = logData.processTimestamp;
  mThisPointer = thisPointer;

  updateState(1,
    storeKeyList("Flintstones", this, std::string("Jetsons")),
    variableNames("Fred", "Foo", "Wilma"), 
    [](DataStoreStateArray<3>& statesInOut){
      statesInOut[0] = "Barney";
      statesInOut[1] = std::nullopt;
      statesInOut[2] = "I'm Home";
    }
  );
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

  size_t index = 0;
  while(index < messageBuffer.size()) {
    // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] LOG: " 
    << messageBuffer.substr(index, LOG_LINE_CHARACTER_LIMIT) << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
    index += LOG_LINE_CHARACTER_LIMIT;
  }
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

void BlockLogger::printUpdateState(int line, const std::string& storeKey, const std::string& varName, std::optional<std::string>& value) {
  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "UPDATE STATE: "
  << "StoreKey='" << storeKey << "' : StateName='" << varName << "' : Value='" << value.value_or("N/A") << "'" << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::setState(int line, const void* address, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setState(address, stateName, stateUpdater);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
    << "Address=" << address << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::printCurrentState(int line, const void* address, const std::string& stateName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> state = loggerDataStore.getState(address, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
  << "Address=" << address << " : StateName='" << stateName << "' : Value='" << (state ? state.value() : "NO STATE") << "'"
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printAllCurrentState(int line, const void* address) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto allState = loggerDataStore.getAllState(address);

  for(const auto& row : allState) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
    << "Address=" << address << " : StateName='" << row.first << "' : Value='" << (row.second ? row.second.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseState(int line, const void* address, const std::string& stateName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseState(address, stateName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
    << "Address=" << address << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseAllState(int line, const void* address) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllState(address);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
    << "Address=" << address << " : Deleted State Count=" << deletedStateCount
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

//-------- State Store Name 

void BlockLogger::setStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setStateStoreName(stateStoreName, stateName, stateUpdater);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::printCurrentStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> state = loggerDataStore.getStateStoreName(stateStoreName, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
  << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << (state ? state.value() : "NO STATE") << "'"
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printAllCurrentStateOnStoreName(int line, const std::string& stateStoreName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto allState = loggerDataStore.getAllStateStoreName(stateStoreName);

  for(const auto& row : allState) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << row.first << "' : Value='" << (row.second ? row.second.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseStateStoreName(stateStoreName, stateName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'" 
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseAllStateOnStoreName(int line, const std::string& stateStoreName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllStateStoreName(stateStoreName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
    << "StoreName=" << stateStoreName << " : Deleted State Count=" << deletedStateCount
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

//------------------

BlockLogger::~BlockLogger() {
  if(!mEnabledMode) {
    return;
  }

  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
    << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

uint32_t BlockLogger::getEnabledMode() const {
  return mEnabledMode;
}

} // namespace CAP

#endif
