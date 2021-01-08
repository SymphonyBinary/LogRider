#include "dereklogger.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include <cstdint>

static const char* colourArray[] = {
  COLOUR BOLD C_RED,
  COLOUR BOLD C_GREEN,
  COLOUR BOLD C_YELLOW,
  COLOUR BOLD C_BLUE,
  COLOUR BOLD C_MAGENTA,
  COLOUR BOLD C_CYAN,
  COLOUR BOLD C_WHITE,
};
static const int colourArraySize = sizeof(colourArray)/sizeof(colourArray[0]);

//Before, I was using thread_local to store the per-thread state so that I wouldn't have to deal with mutexes or anything
//somehow on Android, thread_local didn't really do the expected things.  Also the process is

namespace {
  void printTab(std::stringstream& ss, unsigned int threadId, unsigned int depth) {
    ss << COLOUR BOLD C_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " <<
      colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD C_GREEN << " ";

    for(unsigned int i = 0 ; i < depth; ++i ){
      ss << TAB_DELIMITER;
    }
  }
}

struct LoggerData {
  unsigned int logDepth = 0;
  unsigned int perThreadUniqueFunctionIdx = 0;
  unsigned int relativeThreadIdx = 0;
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
  BlockLoggerDataStore(){}

  std::mutex mMut;
  std::unordered_map<std::thread::id, LoggerData> mData;
  unsigned int mUniqueThreadsSeen = 0;
};

BlockLogger::BlockLogger(const void* thisPointer) {
  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();

  mDepth = logData.logDepth;
  mId = logData.perThreadUniqueFunctionIdx;
  mThreadId = logData.relativeThreadIdx;
  mThisPointer = thisPointer;
}

void BlockLogger::setPrimaryLog(int line, const char* logInfoBuffer, const char* customMessageBuffer) {
  mhasCustomMessage = (customMessageBuffer != nullptr);

  std::memcpy(&mlogInfoBuffer, logInfoBuffer, D_LOG_BUFFER_SIZE);

  std::stringstream ss;
  printTab(ss, mThreadId, mDepth);

  ss << COLOUR BOLD C_GREEN << PRIMARY_LOG_BEGIN_DELIMITER << COLOUR BOLD C_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());

  if(mhasCustomMessage) {
    std::memcpy(&mcustomMessageBuffer, customMessageBuffer, D_LOG_BUFFER_SIZE);
    log(line, mcustomMessageBuffer);
  }
}

void BlockLogger::log(int line, const char* messageBuffer) {
  std::stringstream ss;
  printTab(ss, mThreadId, mDepth);
  ss << COLOUR BOLD C_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD C_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD C_GREEN << line << COLOUR RESET << "] LOG: " << messageBuffer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::error(int line, const char* messageBuffer) {
  std::stringstream ss;
  printTab(ss, mThreadId, mDepth);
  ss << COLOUR BOLD C_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD C_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD C_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD C_RED << "ERROR: " <<  messageBuffer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

BlockLogger::~BlockLogger() {
  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();

  std::stringstream ss;
  printTab(ss, mThreadId, mDepth);
  ss << COLOUR BOLD C_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD C_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}