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
  void printChannel(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelId, std::string_view channelName, bool enabled, int verbosityLevel) {
    ss << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
      << processId << " " << THREAD_ID_DELIMITER << colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD CAP_GREEN 
      << " CHANNEL-ID=" << std::setw(3) << std::setfill('0') << channelId << " : ENABLED=" << (enabled ? "YES :" : "NO  :") << " VERBOSITY=" << verbosityLevel << " : ";

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
}

struct LoggerData {
  unsigned int logDepth = 0;
  unsigned int perThreadUniqueFunctionIdx = 0;
  unsigned int relativeThreadIdx = 0;
  unsigned int processTimestamp = 0;
};

template<class KeyType>
class StateStore {
public:
  const std::string& setState(KeyType objectId, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    return (mStateStore[objectId][stateName] = stateUpdater(mStateStore[objectId][stateName])).value();
  }

  std::optional<std::string> getState(KeyType objectId, const std::string& stateName) const {
    if (const auto& objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      const auto& objectStateMap = objectFind->second;
      if (const auto& stateFind = objectStateMap.find(stateName); stateFind != objectStateMap.end()) {
        return stateFind->second;
      }
    }
    return std::nullopt;
  }

  // intentionally returning by copy for threading reasons.  Be careful if this is too large.
  std::unordered_map<std::string, std::optional<std::string>> getAllState(KeyType objectId) const {
    if (const auto& objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      return objectFind->second;
    }
    return {};
  }

  std::optional<std::string> releaseState(KeyType objectId, const std::string& stateName) {
    std::optional<std::string> deletedValueRet;
    if (auto objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      auto& objectStateMap = objectFind->second;
      if (auto stateFind = objectStateMap.find(stateName); stateFind != objectStateMap.end()) {
        deletedValueRet = stateFind->second;
        objectStateMap.erase(stateFind);
      }
    }
    return deletedValueRet;
  }

  int releaseAllState(KeyType objectId) {
    int stateDeletedCountRet = 0;
    if (auto objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      auto& objectStateMap = objectFind->second;
      stateDeletedCountRet = objectStateMap.size();
      mStateStore.erase(objectFind);
    }
    return stateDeletedCountRet;
  }

private:
  std::unordered_map<KeyType, std::unordered_map<std::string, std::optional<std::string>>> mStateStore;
};

struct BlockLoggerDataStore {
  static BlockLoggerDataStore& getInstance() {
    static BlockLoggerDataStore instance;
    //PRINT_TO_LOG("Singleton instance: %p", (void*)&instance);
    return instance;
  }
  
  LoggerData newBlockLoggerInstance() {
    const std::lock_guard<std::mutex> guard(mMut);

    auto threadId = std::this_thread::get_id();

    bool newThreadId = (mData.find(threadId) == mData.end());
    //PRINT_TO_LOG("newThreadId = %s", newThreadId ? "true" : "false");

    auto& counters = mData[threadId];
    if(!newThreadId) {
      ++counters.logDepth;
      ++counters.perThreadUniqueFunctionIdx;
    } else {
      counters.processTimestamp = mProcessTimestamp;
      counters.relativeThreadIdx = mUniqueThreadsSeen++;
    }

    return counters;
  }

  void removeBlockLoggerInstance(){
    const std::lock_guard<std::mutex> guard(mMut);

    auto threadId = std::this_thread::get_id();
    auto& data = mData.at(threadId); //will throw if it can't find, which we want
    --data.logDepth;
  }

  const std::string& setState(const void* objectId, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.setState(objectId, stateName, stateUpdater);
  }

  std::optional<std::string> getState(const void* objectId, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.getState(objectId, stateName);
  }

  std::unordered_map<std::string, std::optional<std::string>> getAllState(const void* objectId) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.getAllState(objectId);
  }

  std::optional<std::string> releaseState(const void* objectId, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.releaseState(objectId, stateName);
  }

  int releaseAllState(const void* objectId) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.releaseAllState(objectId);
  }

  const std::string& setStateStoreName(const std::string& storeName, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.setState(storeName, stateName, stateUpdater);
  }

  std::optional<std::string> getStateStoreName(const std::string& storeName, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.getState(storeName, stateName);
  }

  std::unordered_map<std::string, std::optional<std::string>> getAllStateStoreName(const std::string& storeName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.getAllState(storeName);
  }

  std::optional<std::string> releaseStateStoreName(const std::string& storeName, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.releaseState(storeName, stateName);
  }

  int releaseAllStateStoreName(const std::string& storeName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.releaseAllState(storeName);
  }

  BlockLoggerDataStore(const BlockLoggerDataStore&) = delete;
  void operator=(const BlockLoggerDataStore&) = delete;

private:
  BlockLoggerDataStore() 
  : mProcessTimestamp(static_cast<unsigned int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count())) {
  }

  std::mutex mMut;
  std::unordered_map<std::thread::id, LoggerData> mData;
  unsigned int mUniqueThreadsSeen = 0;
  //special timestamp used as a key to identify this process from others.
  const unsigned int mProcessTimestamp = 0;

  // for set/get/release commands.  Used for addresses.  Care must be taken not to send literal strings to this (which are just char*)
  StateStore<const void*> mCustomLogStatePointers;

  // for set/get/release commands.  Used for strings.
  StateStore<std::string> mCustomLogStateStoreNames;
};

/*static*/ BlockChannelTree& BlockChannelTree::getInstance() {
  static BlockChannelTree instance;
  return instance;
}

bool BlockChannelTree::isChannelEnabled(CAP::CHANNEL channel) {
  return mEnabledChannelsById[(size_t)channel];
}

BlockChannelTree::BlockChannelTree() {
  PRINT_TO_LOG("%s", "CAPTAIN'S LOG - VERSION 1.1"); \

  std::vector<char> enabledStack = {true};
  int index = 0;
  bool currentEnabled = true;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabled) \
  currentEnabled = enabled && enabledStack.back(); \
  mEnabledChannelsById[index++] = currentEnabled;

  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  enabledStack.emplace_back(currentEnabled);

  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  enabledStack.pop_back();

  #include "../channels/channeldefs.hpp"

  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN


  // Now print it.  This should be the first thing caplog prints.
  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();
  int channelDepth = 0;
  int channelId = 0;
  std::stringstream ss;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabled) \
  printChannel(ss, logData.processTimestamp, logData.relativeThreadIdx, channelDepth, channelId++, #name, enabled, verboseLevel); \
  PRINT_TO_LOG("%s", ss.str().c_str()); \
  ss.str("");
  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  ++channelDepth;
  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  --channelDepth;

  #include "../channels/channeldefs.hpp"
  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
//void printChannel(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelID, std::string_view channelName) {
  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
}


BlockLogger::BlockLogger(const void* thisPointer, CAP::CHANNEL channel)
  : mEnabled(BlockChannelTree::getInstance().isChannelEnabled(channel)) 
  , mChannel(channel) {
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();

  mDepth = logData.logDepth;
  mId = logData.perThreadUniqueFunctionIdx;
  mThreadId = logData.relativeThreadIdx;
  mProcessId = logData.processTimestamp;
  mThisPointer = thisPointer;
}

void BlockLogger::setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer) {
  if(!mEnabled) {
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
  if(!mEnabled) {
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
  if(!mEnabled) {
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

void BlockLogger::setState(int line, const void* address, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setState(address, stateName, stateUpdater);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
  << "Address=" << address << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printCurrentState(int line, const void* address, const std::string& stateName) {
  if(!mEnabled) {
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
  if(!mEnabled) {
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
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseState(address, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
  << "Address=" << address << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'"
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::releaseAllState(int line, const void* address) {
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllState(address);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
  << "Address=" << address << " : Deleted State Count=" << deletedStateCount
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

//-------- State Store Name 

void BlockLogger::setStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setStateStoreName(stateStoreName, stateName, stateUpdater);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
  << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printCurrentStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName) {
  if(!mEnabled) {
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
  if(!mEnabled) {
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
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseStateStoreName(stateStoreName, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
  << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'" 
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::releaseAllStateOnStoreName(int line, const std::string& stateStoreName) {
  if(!mEnabled) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllStateStoreName(stateStoreName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
  << "StoreName=" << stateStoreName << " : Deleted State Count=" << deletedStateCount
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

//------------------

BlockLogger::~BlockLogger() {
  if(!mEnabled) {
    return;
  }

  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

bool BlockLogger::isEnabled() {
  return mEnabled;
}

} // namespace CAP
