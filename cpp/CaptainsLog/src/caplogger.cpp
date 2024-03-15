#ifdef ENABLE_CAP_LOGGER

#include "include/caplogger.hpp"
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>
#include <assert.h>

namespace CAP {

namespace {

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


struct PrintPrefix {
  unsigned int processId;
  unsigned int threadId;
  unsigned int channelId;
};

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

  if (completeOutputString.size() < LOG_LINE_CHARACTER_LIMIT) {
    PRINT_TO_LOG("%s", completeOutputString.c_str());
  } else {
    std::stringstream concatBeginStream;
    concatBeginStream << PrintPrefix{processId, threadId, channelId} << CONCAT_DELIMITER_BEGIN;
    const std::string concatBeginString = concatBeginStream.str();
    size_t concatBeginLength = concatBeginString.size();
    size_t substrMax = LOG_LINE_CHARACTER_LIMIT - concatBeginLength;
    std::string currentLine = concatBeginString + completeOutputString.substr(0, substrMax);
    size_t index = substrMax;
    PRINT_TO_LOG("%s", currentLine.c_str());

    std::stringstream concatContinueStream;
    concatContinueStream << PrintPrefix{processId, threadId, channelId} << CONCAT_DELIMITER_CONTINUE;
    const std::string concatContinueString = concatContinueStream.str();
    size_t concatContinueLength = concatContinueString.size();
    assert(concatContinueLength < LOG_LINE_CHARACTER_LIMIT);
    substrMax = LOG_LINE_CHARACTER_LIMIT - concatContinueLength;
    while(index < completeOutputString.size()) {
      std::string currentLine = concatContinueString + completeOutputString.substr(index, substrMax);
      index += substrMax;
      PRINT_TO_LOG("%s", currentLine.c_str());
    }

    std::stringstream concatEndStream;
    concatEndStream << PrintPrefix{processId, threadId, channelId} << CONCAT_DELIMITER_END;
    const std::string concatEndString = concatEndStream.str();
    PRINT_TO_LOG("%s", concatEndString.c_str());
  }
}
} // namespace


BlockLogger::BlockLogger(const void* thisPointer, CAP::CHANNEL channel)
  : mEnabledMode(CAP::getChannelFlagMap()[(size_t)channel])
  , mlogInfoBuffer()
  , mcustomMessageBuffer()
  , mId(0)
  , mDepth(0)
  , mThreadId(0)
  , mProcessId(0)
  , mChannel(channel)
  , mThisPointer(thisPointer) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    //If this block is silent and can't write to output, no need to record 
    //depth, id, etc which are used for printing to the log.
    auto logData = BlockLoggerDataStore::getInstance().newBlockLoggerInstance();
    mDepth = logData.logDepth;
    mId = logData.perThreadUniqueFunctionIdx;
    mThreadId = logData.relativeThreadIdx;
    mProcessId = logData.processTimestamp;
  }
}

void BlockLogger::setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    mlogInfoBuffer = std::move(logInfoBuffer);

    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_BEGIN_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer << " "
    << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

    // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
    if(customMessageBuffer.size() > 1) {
      log(line, std::move(customMessageBuffer));
    }
  }
}

void BlockLogger::log(int line, std::string_view messageBuffer) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] LOG: " 
    << messageBuffer << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
  }
}

void BlockLogger::error(int line, std::string_view messageBuffer) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "ERROR: " 
    << messageBuffer << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
  }
}

//-------- State Addresses
void BlockLogger::printState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    auto states = BlockLoggerDataStore::getInstance().getStates(storeKeyList(key), variableNames(varName));
    printStateImpl(line, "PRINT STATE", to_string(key), varName, states[0]);
  }
}

void BlockLogger::printAllStateOfStore(int line, const DataStoreKey& key) {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    auto allStates = BlockLoggerDataStore::getInstance().getAllStates(key);

    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED 
    << "PRINTING ALL STATE IN STORE: StoreKey='" << to_string(key) << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

    for(const auto& row : allStates) {
      printStateImpl(line, "PRINT STATE", to_string(key), row.first, row.second);
    }
  }
}

void BlockLogger::releaseState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName) {
  if(!(mEnabledMode)) {
    return;
  }

  std::optional<std::string> oldState = BlockLoggerDataStore::getInstance().releaseState(key, varName);

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    printStateImpl(line, "RELEASE STATE", to_string(key), varName, oldState);
  }
}

void BlockLogger::releaseAllStateOfStore(int line, const DataStoreKey& key) {
  if(!(mEnabledMode)) {
    return;
  }

  int deletedCount = BlockLoggerDataStore::getInstance().releaseAllStates(key);

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED 
    << "RELEASE ALL STATE IN STORE: StoreKey='" << to_string(key) << "' NumDeleted='" << deletedCount << "'" << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
  }
}

void BlockLogger::printStateImpl(int line, const std::string& logCommand, const std::string& storeKey, const std::string& varName, const std::optional<std::string>& value) {
  std::stringstream ss;
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << logCommand << ": "
  << "StoreKey='" << storeKey << "' : StateName='" << varName << "' : Value='" << value.value_or("N/A") << "'" << COLOUR RESET;
  writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
}

BlockLogger::~BlockLogger() {
  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    // block logger instance is only created when logging/output mode enabled.
    BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
    std::stringstream ss;
    ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer << " "
    << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
  }
}

} // namespace CAP

#endif
