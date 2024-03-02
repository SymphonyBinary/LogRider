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
      << " CHANNEL-ID=" << std::setw(3) << std::setfill('0') << channelId << " : ENABLED=" << (enabled ? "YES :" : "NO  :") << " VERBOSITY : " << verbosityLevel << " : ";

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

  #include "channeldefs.hpp"

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

  #include "channeldefs.hpp"
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

void BlockLogger::set(int line, std::string_view name, std::string_view value) {
  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET: " <<  name << " = " << value << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

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