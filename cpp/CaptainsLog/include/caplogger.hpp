#pragma once

#ifdef ENABLE_CAP_LOGGER

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <limits>
#include <optional>
#include <variant>

#include <cstdint>

#include "capdata.hpp"
#include "channels.hpp"
#include "utilities.hpp"

#define LOG_LINE_CHARACTER_LIMIT 150
#define LOG_INFO_BUFFER_LIMIT 100

/*
The " " here
snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
is because you will get warnings if you try to pass a zero sized string to sprintf.
Ideally, you want to branch if __VA_ARGS__ is empty and call setPrimaryLog without passing it 
a buffer at all
*/
#define CAP_LOG_INTERNAL(pointer, channel, ...) \
  [[maybe_unused]] constexpr bool channelCompileNotDisabled = CAP::getChannelFlagMap()[(size_t)channel]; \
  [[maybe_unused]] constexpr bool channelCompileEnabledOutput = CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_OUTPUT; \
  [[maybe_unused]] constexpr bool channelCompileEnabledState = CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_STATE; \
  CAP::BlockLogger blockScopeLog{pointer, channel}; \
  if constexpr (channelCompileEnabledOutput) { \
    std::stringstream ss; \
    ss << COLOUR RESET " [" COLOUR BOLD CAP_GREEN << __LINE__ <<  COLOUR RESET "]::[" \
      COLOUR BOLD CAP_CYAN << __FILENAME__ << COLOUR RESET "]::[" \
      COLOUR BOLD CAP_MAGENTA << __PRETTY_FUNCTION__ << COLOUR RESET "]"; \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.setPrimaryLog(__LINE__, ss.str(), buffer); \
    delete[] buffer; \
  } else if constexpr (channelCompileEnabledState) { \
    blockScopeLog.setPrimaryLog(__LINE__, "", ""); \
  }

/// you may optionally provide an argument in the form of "(format, ...)"
#define CAP_LOG_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG_CHANNEL_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_CHANNEL_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG(...) \
  if constexpr (channelCompileEnabledOutput) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.log(__LINE__, buffer); \
    delete[] buffer; \
  }

#define CAP_LOG_ERROR(...) \
  if constexpr (channelCompileEnabledOutput) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.error(__LINE__, buffer); \
    delete[] buffer; \
  }

// https://stackoverflow.com/questions/36030589/i-cannot-pass-lambda-as-stdfunction
// updaterLambda is of form std::function<std::string(std::optional<std::string>)>;
#define CAP_LOG_UPDATE_STATE_ON(storeKeys, variableNames, updaterLambda) \
  if constexpr (channelCompileEnabledState) { \
    CAP::DataStoreValuesArrayUpdater<decltype(storeKeys)::Size> updaterFunc = updaterLambda; \
    blockScopeLog.updateState(__LINE__, storeKeys, variableNames, updaterFunc); \
  }

#define CAP_LOG_PRINT_STATE_ON(storeKey, name) \
  if constexpr (channelCompileEnabledOutput) { \
    blockScopeLog.printState(__LINE__, storeKey, name); \
  }

#define CAP_LOG_PRINT_ALL_STATE_ON(storeKey) \
  if constexpr (channelCompileEnabledOutput) { \
    blockScopeLog.printAllStateOfStore(__LINE__, storeKey); \
  }

#define CAP_LOG_RELEASE_STATE_ON(storeKey, name) \
  if constexpr (channelCompileEnabledState) { \
    blockScopeLog.releaseState(__LINE__, storeKey, name); \
  }

#define CAP_LOG_RELEASE_ALL_STATE_ON(storeKey) \
  if constexpr (channelCompileEnabledState) { \
    blockScopeLog.releaseAllStateOfStore(__LINE__, storeKey); \
  }

#define CAP_LOG_UPDATE_STATE(name, updaterLambda) CAP_LOG_UPDATE_STATE_ON(CAP::storeKeyList(this), CAP::variableNames(name), updaterLambda)
#define CAP_LOG_PRINT_STATE(name) CAP_LOG_PRINT_STATE_ON(this, name)
#define CAP_LOG_PRINT_ALL_STATE(name) CAP_LOG_PRINT_ALL_STATE_ON(this)
#define CAP_LOG_RELEASE_STATE(name) CAP_LOG_RELEASE_STATE_ON(this, name)
#define CAP_LOG_RELEASE_ALL_STATE() CAP_LOG_RELEASE_ALL_STATE_ON(this)

#define CAP_LOG_EXECUTE_LAMBDA(lambda) \
  if constexpr (channelCompileNotDisabled) { \
    std::function<void()> func = lambda; \
    func(); \
  }

namespace CAP {

class BlockLogger {
public:
  BlockLogger(const void* thisPointer, CAP::CHANNEL channel);
  ~BlockLogger();

  void setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer);

  void log(int line, std::string_view messageBuffer);

  void error(int line, std::string_view messageBuffer);

  // UpdaterFunc is a callable that receives DataStoreStateArray<DATA_COUNT>&
  // as its only input parameter.  Return is unused/ignored.
  template<size_t DATA_COUNT, class UpdaterFunc>
  void updateState(
      int line, 
      const DataStoreKeysArrayN<DATA_COUNT>& keys, 
      const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& varNames,
      const UpdaterFunc& stateUpdater) {
    if(!mEnabledMode) {
      return;
    }

    auto& loggerDataStore = BlockLoggerDataStore::getInstance();
    auto states = loggerDataStore.getStates(keys, varNames);
    stateUpdater(states);

    decltype(states) statesPrintCopy;
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
      statesPrintCopy = states;
    }

    auto changes = loggerDataStore.updateStates(keys, varNames, std::move(states));

    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
      for (size_t i = 0; i < DATA_COUNT; ++i) {
        std::string comLine = "UPDATE STATE(" + to_string(changes[i]) + ") ";
        printStateImpl(line, comLine, to_string(keys[i]), varNames[i], statesPrintCopy[i]);
      }
    }    
  }

  void printState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName);
  void printAllStateOfStore(int line, const DataStoreKey& key);

  void releaseState(int line, const DataStoreKey& keys, const std::string& stateName);
  void releaseAllStateOfStore(int line, const DataStoreKey& key);

private:
  void printStateImpl(int line, const std::string& logCommand, const std::string& storeKey, const std::string& varName, const std::optional<std::string>& value);

  const uint32_t mEnabledMode = FULLY_DISABLED;
  std::string mlogInfoBuffer;
  std::string mcustomMessageBuffer;
  unsigned int mId;
  unsigned int mDepth;
  unsigned int mThreadId;
  unsigned int mProcessId;
  CAP::CHANNEL mChannel;
  const void* mThisPointer;
};

} // namespace CAP

#else

#define CAP_LOG_BLOCK(...)
#define CAP_LOG_BLOCK_NO_THIS(...)

#define CAP_LOG(...)
#define CAP_LOG_ERROR(...)

#define CAP_LOG_UPDATE_STATE_ON(...)
#define CAP_LOG_PRINT_STATE_ON(...)
#define CAP_LOG_PRINT_ALL_STATE_ON(...)
#define CAP_LOG_RELEASE_STATE_ON(...)
#define CAP_LOG_RELEASE_ALL_STATE_ON(...)

#define CAP_LOG_UPDATE_STATE(...)
#define CAP_LOG_PRINT_STATE(...)
#define CAP_LOG_PRINT_ALL_STATE(...)
#define CAP_LOG_RELEASE_STATE(...)
#define CAP_LOG_RELEASE_ALL_STATE(...)

#define CAP_LOG_EXECUTE_LAMBDA(...)

// #define CAP_LOG_INTEGRATION_TEST_REPORT(...)

#endif
