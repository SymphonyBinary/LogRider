#pragma once

#ifdef ENABLE_CAP_LOGGER

#ifndef CHANNELS_PATH
static_assert(false, "CHANNELS_PATH not defined");
#endif

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

#include "colors.hpp"

#define LOG_LINE_CHARACTER_LIMIT 150
#define LOG_INFO_BUFFER_LIMIT 100

#define CAPTAINS_LOG_STRINGIFY2(X) #X
#define CAPTAINS_LOG_STRINGIFY(X) CAPTAINS_LOG_STRINGIFY2(X)

#ifdef SHOW_THREAD_ID
  #define INSERT_THREAD_ID COLOUR CAP_BLUE << std::this_thread::get_id() << COLOUR RESET
#else
  #define INSERT_THREAD_ID ""
#endif

#ifdef ANDROID
  #include <android/log.h>
  #define  CAPLOG_TAG    "D_LOG::"
  #define  PRINT_TO_LOG(...)  __android_log_print(ANDROID_LOG_DEBUG, "CAPLOG_TAG", __VA_ARGS__)
#else
  #define PRINT_TO_LOG(...) printf(__VA_ARGS__); printf("\n");
#endif

//https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

///////
//https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick/11172679#11172679
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
    SELECT_20TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_20TH(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20, ...) a20
///////

// Note, prefer to use CAP::string(...) instead. eg. CAP::string("VarBase", 1);
#define CAP_LOG_FSTRING_HELPER(...) \
  []() -> std::string { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    std::string buffer; \
    buffer.resize(needed); \
    snprintf(&buffer[0], needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    delete[] buffer; \
    return buffer; \
  }();
/*
The " " here
snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
is because you will get warnings if you try to pass a zero sized string to sprintf.
Ideally, you want to branch if __VA_ARGS__ is empty and call setPrimaryLog without passing it 
a buffer at all
*/
#define CAP_LOG_INTERNAL(pointer, channel, ...) \
  CAP::BlockLogger blockScopeLog{pointer, channel}; \
  if (blockScopeLog.getEnabledMode() & CAP::CAN_WRITE_TO_OUTPUT) { \
    std::stringstream ss; \
    ss << COLOUR RESET " [" COLOUR BOLD CAP_GREEN << __LINE__ <<  COLOUR RESET "]::[" \
      COLOUR BOLD CAP_CYAN << __FILENAME__ << COLOUR RESET "]::[" \
      COLOUR BOLD CAP_MAGENTA << __PRETTY_FUNCTION__ << COLOUR RESET "]"; \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.setPrimaryLog(__LINE__, ss.str(), buffer); \
    delete[] buffer; \
  } else if (blockScopeLog.getEnabledMode() & CAP::CAN_WRITE_TO_STATE) { \
    blockScopeLog.setPrimaryLog(__LINE__, "", ""); \
  }

/// you may optionally provide an argument in the form of "(format, ...)"
#define CAP_LOG_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG_CHANNEL_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_CHANNEL_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG(...) \
  if (blockScopeLog.getEnabledMode() & CAP::CAN_WRITE_TO_OUTPUT) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.log(__LINE__, buffer); \
    delete[] buffer; \
  }

#define CAP_LOG_ERROR(...) \
  if (blockScopeLog.getEnabledMode() & CAP::CAN_WRITE_TO_OUTPUT) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.error(__LINE__, buffer); \
    delete[] buffer; \
  }

// https://stackoverflow.com/questions/36030589/i-cannot-pass-lambda-as-stdfunction
// updaterLambda is of form CAP::StateUpdaterFunc
#define CAP_LOG_UPDATE_STATE_ON(storeKey, name, updaterLambda) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    CAP::StateUpdaterFunc updaterFunc = updaterLambda; \
    blockScopeLog.setState(__LINE__, address, name, updaterFunc); \
  }

#define CAP_LOG_PRINT_STATE_ON(storeKey, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printCurrentState(__LINE__, address, name); \
  }

#define CAP_LOG_PRINT_All_STATE_ON(storeKey) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printAllCurrentState(__LINE__, address); \
  }

#define CAP_LOG_RELEASE_STATE_ON(storeKey, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseState(__LINE__, address, name); \
  }

#define CAP_LOG_RELEASE_ALL_STATE_ON(storeKey) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseAllState(__LINE__, address); \
  }  

// https://stackoverflow.com/questions/36030589/i-cannot-pass-lambda-as-stdfunction
// updaterLambda is of form CAP::StateUpdaterFunc
#define CAP_LOG_SET_STATE_ON_ADDRESS(address, name, updaterLambda) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    CAP::StateUpdaterFunc updaterFunc = updaterLambda; \
    blockScopeLog.setState(__LINE__, address, name, updaterFunc); \
  }

#define CAP_LOG_PRINT_STATE_ON_ADDRESS(address, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printCurrentState(__LINE__, address, name); \
  }

#define CAP_LOG_PRINT_All_STATE_ON_ADDRESS(address) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printAllCurrentState(__LINE__, address); \
  }

#define CAP_LOG_RELEASE_STATE_ON_ADDRESS(address, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseState(__LINE__, address, name); \
  }

#define CAP_LOG_RELEASE_ALL_STATE_ON_ADDRESS(address) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseAllState(__LINE__, address); \
  }  

#define CAP_LOG_SET_STATE(name, ...) CAP_LOG_SET_STATE_ON_ADDRESS(this, name, __VA_ARGS__)
#define CAP_LOG_PRINT_STATE(name) CAP_LOG_PRINT_STATE_ON_ADDRESS(this, name)
#define CAP_LOG_PRINT_ALL_STATE(name) CAP_LOG_PRINT_All_STATE_ON_ADDRESS(this)
#define CAP_LOG_RELEASE_STATE(name) CAP_LOG_RELEASE_STATE_ON_ADDRESS(this, name)
#define CAP_LOG_RELEASE_ALL_STATE() CAP_LOG_RELEASE_ALL_STATE_ON_ADDRESS(this)

#define CAP_LOG_SET_STATE_ON_STORE_NAME(stateStoreName, name, updaterLambda) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    CAP::StateUpdaterFunc updaterFunc = updaterLambda; \
    blockScopeLog.setStateOnStoreName(__LINE__, stateStoreName, name, updaterFunc); \
  }

#define CAP_LOG_PRINT_STATE_ON_STORE_NAME(stateStoreName, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printCurrentStateOnStoreName(__LINE__, stateStoreName, name); \
  }

#define CAP_LOG_PRINT_ALL_STATE_ON_STORE_NAME(stateStoreName) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.printAllCurrentStateOnStoreName(__LINE__, stateStoreName); \
  }

#define CAP_LOG_RELEASE_STATE_ON_STORE_NAME(stateStoreName, name) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseStateOnStoreName(__LINE__, stateStoreName, name); \
  }

#define CAP_LOG_RELEASE_ALL_STATE_ON_STORE_NAME(stateStoreName) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    blockScopeLog.releaseAllStateOnStoreName(__LINE__, stateStoreName); \
  }  

#define CAP_LOG_EXECUTE_LAMBDA(function) \
  if (blockScopeLog.getEnabledMode() & CAP::ALL_FLAGS) { \
    function(); \
  }

namespace CAP {

// input parameter is the previous value if it exists
using StateUpdaterFunc = std::function<std::string(std::optional<std::string>)>;

enum class CHANNEL {
  #define CAPTAINS_LOG_CHANNEL(name, ...) name,
  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
  #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
  COUNT,
};

inline std::string_view channelToString(CHANNEL channel) {
  static std::array<std::string, (size_t)CHANNEL::COUNT> strings = {
    #define CAPTAINS_LOG_CHANNEL(name, ...) #name,
    #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
    #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
    #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
    #undef CAPTAINS_LOG_CHANNEL
    #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
    #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
  };
  return strings[(size_t)channel];
}

enum ChannelEnabledFlags : uint32_t {
  CAN_WRITE_TO_STATE = 1 << 1,
  CAN_WRITE_TO_OUTPUT = 1 << 2,
  ALL_FLAGS = std::numeric_limits<uint32_t>::max(),
};

enum ChannelEnabledMode : uint32_t {
  FULLY_DISABLED = 0,
  FULLY_ENABLED = ALL_FLAGS,
  ENABLED_NO_OUTPUT = ALL_FLAGS ^ CAN_WRITE_TO_OUTPUT,
};

inline uint32_t getChannelFlags(CHANNEL channel) {
  static std::array<uint32_t, (size_t)CHANNEL::COUNT> flags = {
    #define CAPTAINS_LOG_CHANNEL(name, verbosity, executionLevel) executionLevel,
    #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
    #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
    #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
    #undef CAPTAINS_LOG_CHANNEL
    #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
    #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
  };
  return flags[(size_t)channel];
}

template<class T>
std::string stringify(T&& t) {
    //if std::string or literal string, just return it (with implicit conversion for literal string)
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, const char*>){
        return t;
    } else {
        return std::to_string(t);
    }
}

template<class... Args>
std::string string(Args... args) {
  return (stringify(args) + ...);
}


// template<class T, unsigned int N>
// struct ArrayN {
//   ArrayN(std::array<T, N> array)
//       : v(std::move(array)){}
//   constexpr static unsigned int Size = N;
//   std::array<T, N> v;
// };

// the method helpers (like storeKeyList) exist because macros
// don't like comma separated values in aggregate initializers, like
// std::array{1,2,3} and will treat them like macro arguments.

// DataStores are "contexts" for variables.  They are analagous to objects,
// this is why there's an easy path to create them from the "this" pointer.
// using DataStoreKey = std::variant<const void*, const char*, std::string>;
// template<unsigned int DATA_COUNT>
// using DataStoreKeysArrayN = ArrayN<DataStoreKey, DATA_COUNT>;

// template<typename... Args>
// DataStoreKeysArrayN<sizeof...(Args)> storeKeyList(const Args&... args) {
//   return ArrayN<DataStoreKey, sizeof...(Args)>({args...});
// }

// // DataStoreMemberVariableName are analagous to member variables of objects
// using DataStoreMemberVariableName = std::string;
// template<unsigned int DATA_COUNT>
// using DataStoreMemberVariableNamesArrayN = ArrayN<DataStoreMemberVariableName, DATA_COUNT>;

// template<typename... Args>
// DataStoreMemberVariableNamesArrayN<sizeof...(Args)> variableNames(const Args&... args) {
//   return ArrayN<DataStoreMemberVariableName, sizeof...(Args)>({args...});
// }

// using DataStoreMutableState = std::optional<std::string>*;
// template<unsigned int DATA_COUNT>
// using DataStoreMutableStateArray = std::array<DataStoreMutableState, DATA_COUNT>;
// template<unsigned int DATA_COUNT>
// using DataStoreValuesArrayUpdater = std::function<void(DataStoreMutableStateArray<DATA_COUNT>&)>;


using DataStoreKey = std::variant<const void*, const char*, std::string>;
template<size_t DATA_COUNT>
using DataStoreKeysArray = std::array<DataStoreKey, DATA_COUNT>;

template<typename... Args>
DataStoreKeysArray<sizeof...(Args)> storeKeyList(const Args&... args) {
  return DataStoreKeysArray<sizeof...(Args)>{{args...}};
}

// DataStoreMemberVariableName are analagous to member variables of objects
using DataStoreMemberVariableName = std::string;
template<size_t DATA_COUNT>
using DataStoreMemberVariableNamesArray = std::array<DataStoreMemberVariableName, DATA_COUNT>;

template<typename... Args>
DataStoreMemberVariableNamesArray<sizeof...(Args)> variableNames(const Args&... args) {
  return DataStoreMemberVariableNamesArray<sizeof...(Args)>{{args...}};
}

using DataStoreState = std::optional<std::string>;
template<size_t DATA_COUNT>
using DataStoreStateArray = std::array<DataStoreState, DATA_COUNT>;
template<size_t DATA_COUNT>
using DataStoreValuesArrayUpdater = std::function<void(DataStoreStateArray<DATA_COUNT>&)>;

// // DataStores are "contexts" for variables.  They are analagous to objects,
// // this is why there's an easy path to create them from the "this" pointer.
// using DataStoreKey = std::variant<const void*, const char*, std::string>;
// template<unsigned int DATA_COUNT>
// using DataStoreKeysArray = std::array<DataStoreKey, DATA_COUNT>;

// // DataStoreMemberVariableName are analagous to member variables of objects
// using DataStoreMemberVariableName = const std::string&;
// template<unsigned int DATA_COUNT>
// using DataStoreMemberVariableNamesArray = std::array<DataStoreMemberVariableName, DATA_COUNT>;

// using DataStoreValue = std::optional<std::string>;
// template<unsigned int DATA_COUNT>
// using DataStoreValueArray = std::array<DataStoreValue, DATA_COUNT>;
// template<unsigned int DATA_COUNT>
// using DataStoreValuesArrayUpdater = std::function<DataStoreValueArray<DATA_COUNT>(DataStoreValueArray<DATA_COUNT>)>;


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
    const DataStoreKeysArray<DATA_COUNT>& keys, 
    const DataStoreMemberVariableNamesArray<DATA_COUNT>& varNames,
    const UpdaterFunc& stateUpdater);

  void setState(int line, const void* address, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater);

  void printCurrentState(int line, const void* address, const std::string& stateName);

  void printAllCurrentState(int line, const void* address);

  void releaseState(int line, const void* address, const std::string& stateName);

  void releaseAllState(int line, const void* address);

  void setStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater);

  void printCurrentStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName);

  void printAllCurrentStateOnStoreName(int line, const std::string& stateStoreName);

  void releaseStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName);

  void releaseAllStateOnStoreName(int line, const std::string& stateStoreName);

  uint32_t getEnabledMode() const;

private:
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

class BlockChannelTree {
public:
  static BlockChannelTree& getInstance();
  uint32_t getEnabledMode(CAP::CHANNEL channel);

private:
  BlockChannelTree();
  std::array<uint32_t, (size_t)CAP::CHANNEL::COUNT> mEnabledModeChannelsById;
};

} // namespace CAP

#else

#define CAP_LOG_BLOCK(...)
#define CAP_LOG_BLOCK_NO_THIS(...)

#define CAP_LOG(...)
#define CAP_LOG_ERROR(...)

#define CAP_LOG_SET_STATE_ON_ADDRESS(...)
#define CAP_LOG_PRINT_STATE_ON_ADDRESS(...)
#define CAP_LOG_PRINT_All_STATE_ON_ADDRESS(...)
#define CAP_LOG_RELEASE_STATE_ON_ADDRESS(...)
#define CAP_LOG_RELEASE_ALL_STATE_ON_ADDRESS(...)

#define CAP_LOG_SET_STATE(...)
#define CAP_LOG_PRINT_STATE(...)
#define CAP_LOG_PRINT_All_STATE(...)
#define CAP_LOG_RELEASE_STATE(...)
#define CAP_LOG_RELEASE_ALL_STATE(...)

#define CAP_LOG_SET_STATE_ON_STORE_NAME(...)
#define CAP_LOG_PRINT_STATE_ON_STORE_NAME(...)
#define CAP_LOG_PRINT_ALL_STATE_ON_STORE_NAME(...)
#define CAP_LOG_RELEASE_STATE_ON_STORE_NAME(...)
#define CAP_LOG_RELEASE_ALL_STATE_ON_STORE_NAME(...)

#define CAP_LOG_EXECUTE_LAMBDA(...)

// #define CAP_LOG_INTEGRATION_TEST_REPORT(...)

#endif
