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

#ifdef SHOW_THREAD_ID
  #define INSERT_THREAD_ID COLOUR CAP_BLUE << std::this_thread::get_id() << COLOUR RESET
#else
  #define INSERT_THREAD_ID ""
#endif

#ifdef ANDROID
  #include <android/log.h>
  #define  LOG_TAG    "D_LOG::"
  #define  PRINT_TO_LOG(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
  #define PRINT_TO_LOG(...) printf(__VA_ARGS__); printf("\n");
#endif

//https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define CAP_LOG_BUFFER_SIZE 200

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
#define CAP_LOG_INTERNAL(pointer, ...) \
BlockLogger blockScopeLog{pointer}; \
  { \
    std::stringstream ss; \
    ss << COLOUR RESET " [" COLOUR BOLD CAP_GREEN << __LINE__ <<  COLOUR RESET "]::[" \
      COLOUR BOLD CAP_CYAN << __FILENAME__ << COLOUR RESET "]::[" \
      COLOUR BOLD CAP_MAGENTA << __PRETTY_FUNCTION__ << COLOUR RESET "] "; \
    char blockScopeLogCustomBuffer[CAP_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.setPrimaryLog(__LINE__, ss.str(), blockScopeLogCustomBuffer); \
  }

/// you may optionall provide an argument in the form of "(format, ...)"
#define CAP_LOG_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__) 

#define CAP_LOG(...) \
  { \
    char blockScopeLogCustomBuffer[CAP_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.log(__LINE__, blockScopeLogCustomBuffer); \
  }

#define CAP_ERROR(...) \
  { \
    char blockScopeLogCustomBuffer[CAP_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.error(__LINE__, blockScopeLogCustomBuffer); \
  }

#define CAP_SET(name, ...) \
  { \
    char blockScopeLogCustomBuffer[CAP_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)); \
    blockScopeLog.set(__LINE__, name, blockScopeLogCustomBuffer); \
  }

class BlockLogger {
public:
  BlockLogger(const void* thisPointer);
  ~BlockLogger();

  void setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer);

  void log(int line, std::string_view messageBuffer);

  void error(int line, std::string_view messageBuffer);

  void set(int line, std::string_view name, std::string_view value);

private:
  std::string mlogInfoBuffer;
  std::string mcustomMessageBuffer;
  unsigned int mId;
  unsigned int mDepth;
  unsigned int mThreadId;
  const void* mThisPointer;
};