#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include <cstdint>

#include "colors.hpp"

#define LOG_LINE_CHARACTER_LIMIT 150
#define LOG_INFO_BUFFER_LIMIT 100

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

#ifdef ENABLE_CAP_LOGGER

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

/*
The " " here
snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
is because you will get warnings if you try to pass a zero sized string to sprintf.
Ideally, you want to branch if __VA_ARGS__ is empty and call setPrimaryLog without passing it 
a buffer at all
*/
#define CAP_LOG_INTERNAL(pointer, channel, ...) \
  CAP::BlockLogger blockScopeLog{pointer, channel}; \
  if (blockScopeLog.isEnabled()) { \
    std::stringstream ss; \
    ss << COLOUR RESET " [" COLOUR BOLD CAP_GREEN << __LINE__ <<  COLOUR RESET "]::[" \
      COLOUR BOLD CAP_CYAN << __FILENAME__ << COLOUR RESET "]::[" \
      COLOUR BOLD CAP_MAGENTA << __PRETTY_FUNCTION__ << COLOUR RESET "]"; \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.setPrimaryLog(__LINE__, ss.str(), buffer); \
    delete[] buffer; \
  }

/// you may optionall provide an argument in the form of "(format, ...)"
#define CAP_LOG_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG_CHANNEL_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_CHANNEL_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG(...) \
  if (blockScopeLog.isEnabled()) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.log(__LINE__, buffer); \
    delete[] buffer; \
  }

#define CAP_LOG_ERROR(...) \
  if (blockScopeLog.isEnabled()) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.error(__LINE__, buffer); \
    delete[] buffer; \
  }

#define CAP_SET(name, ...) \
  if (blockScopeLog.isEnabled()) { \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    char* buffer = new char[needed]; \
    snprintf(buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.set(__LINE__, name, buffer); \
    delete[] buffer; \
  }

#else

#define CAP_LOG_BLOCK(...)
#define CAP_LOG_BLOCK_NO_THIS(...)

#define CAP_LOG(...)
#define CAP_LOG_ERROR(...)

// #define CAP_LOG_SET(...);

// #define CAP_LOG_CREATE_STATE_ON_THIS()
// #define CAP_LOG_CREATE_STATE_ON_ADDRESS(...)

// #define CAP_LOG_RELEASE_STATE_ON_THIS(...)
// #define CAP_LOG_RELEASE_STATE_ON_ADDRESS(...)

// #define CAP_LOG_EXECUTE_IF_CHANNEL_ENABLED(...)

// #define CAP_LOG_INLINE_EVALUATE_STATEMENT(...)

// #define CAP_LOG_INTEGRATION_TEST_REPORT(...)

#endif

namespace CAP {

enum class CHANNEL {
  #define CAPTAINS_LOG_CHANNEL(name, ...) name,
  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
  #include "channeldefs.hpp"
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
    #include "channeldefs.hpp"
    #undef CAPTAINS_LOG_CHANNEL
    #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
    #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
  };
  return strings[(size_t)channel];
}

class BlockLogger {
public:
  BlockLogger(const void* thisPointer, CAP::CHANNEL channel);
  ~BlockLogger();

  void setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer);

  void log(int line, std::string_view messageBuffer);

  void error(int line, std::string_view messageBuffer);

  void set(int line, std::string_view name, std::string_view value);

  bool isEnabled();

private:
  bool mEnabled = false;
  std::string mlogInfoBuffer;
  std::string mcustomMessageBuffer;
  unsigned int mId;
  unsigned int mDepth;
  unsigned int mThreadId;
  unsigned int mProcessId;
  const void* mThisPointer;
};

class BlockChannelTree {
public:
  static BlockChannelTree& getInstance();
  bool isChannelEnabled(CAP::CHANNEL channel);

private:
  BlockChannelTree();
  std::array<bool, (size_t)CAP::CHANNEL::COUNT> mEnabledChannelsById;
};

} // namespace CAP
