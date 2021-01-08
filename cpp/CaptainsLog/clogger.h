#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include <cstdint>


//#define COLOURIZE
//#define FANCY_ASCII
//#define SHOW_THREAD_ID
//#define ANDROID //defined outside of this, just here for the comment

//uses ANSI colours https://www.lihaoyi.com/post/BuildyourownCommandLinewithANSIescapecodes.html
#ifdef COLOURIZE
  //https://stackoverflow.com/questions/45526532/c-xcode-how-to-output-color
  #define COLOUR "\033["
  //#define COLOUR= "\u001b[";
  #define BOLD "1;"
  #define RESET "0m"
  #define C_BLACK "30m"
  #define C_RED "31m"
  #define C_GREEN "32m"
  #define C_YELLOW  "33m"
  #define C_BLUE    "34m"
  #define C_MAGENTA "35m"
  #define C_CYAN    "36m"
  #define C_WHITE   "37m"
#else
  #define COLOUR ""
  #define BOLD ""
  #define RESET ""
  #define C_BLACK ""
  #define C_RED ""
  #define C_GREEN ""
  #define C_YELLOW  ""
  #define C_BLUE    ""
  #define C_MAGENTA ""
  #define C_CYAN    ""
  #define C_WHITE   ""
#endif

#ifdef FANCY_ASCII
  // ╔ Unicode: U+2554, UTF-8: E2 95 94
  #define PRIMARY_LOG_BEGIN_DELIMITER "\u2554"
  // ╠ Unicode: U+2560, UTF-8: E2 95 A0
  #define ADD_LOG_DELIMITER "\u2560"
  // ╾ Unicode: U+257E, UTF-8: E2 95 BE
  #define ADD_LOG_SECOND_DELIMITER "\u257E"
  // ╚ Unicode: U+255A, UTF-8: E2 95 9A
  #define PRIMARY_LOG_END_DELIMITER "\u255A"
  // … Unicode: U+2026, UTF-8: E2 80 A6
  //#define MAIN_PREFIX_DELIMITER "\u2026"
  #define MAIN_PREFIX_DELIMITER ""
  // ║ Unicode: U+2551, UTF-8: E2 95 91
  #define TAB_DELIMITER "\u2551"
#else
  #define PRIMARY_LOG_BEGIN_DELIMITER "F"
  #define ADD_LOG_DELIMITER "-"
  #define ADD_LOG_SECOND_DELIMITER ">"
  #define PRIMARY_LOG_END_DELIMITER "L"
  #define MAIN_PREFIX_DELIMITER "C_LOG"
  #define TAB_DELIMITER ":"
#endif

#ifdef SHOW_THREAD_ID
  #define INSERT_THREAD_ID COLOUR C_BLUE << std::this_thread::get_id() << COLOUR RESET
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

#define C_LOG_BUFFER_SIZE 200

//change to __PRETTY_FUNCTION__ if you want the whole function signature
#define C_LOG_COMMON(pointer) \
BlockLogger blockScopeLog{pointer}; \
  { \
    char blockScopeLogInfoBuffer[C_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogInfoBuffer, C_LOG_BUFFER_SIZE, \
      COLOUR RESET " [" COLOUR BOLD C_GREEN "%d" COLOUR RESET "]::[" \
      COLOUR BOLD C_CYAN "%s" COLOUR RESET "]::[" \
      COLOUR BOLD C_MAGENTA "%s" COLOUR RESET "] ", \
      __LINE__, __FILENAME__, __PRETTY_FUNCTION__);

#define C_LOG_WITHOUT_FORMAT(pointer) \
    C_LOG_COMMON(pointer) \
    blockScopeLog.setPrimaryLog(__LINE__, blockScopeLogInfoBuffer, nullptr); \
  }

#define C_LOG_WITH_ONE_STRING(pointer, text) \
    C_LOG_COMMON(pointer) \
    char blockScopeLogCustomBuffer[C_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, C_LOG_BUFFER_SIZE, text); \
    blockScopeLog.setPrimaryLog(__LINE__, blockScopeLogInfoBuffer, blockScopeLogCustomBuffer); \
  }

#define C_LOG_WITH_FORMAT(pointer, ...) \
    C_LOG_COMMON(pointer) \
    char blockScopeLogCustomBuffer[C_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, C_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.setPrimaryLog(__LINE__, blockScopeLogInfoBuffer, blockScopeLogCustomBuffer); \
  }

// #define BAR(...) printf(FIRST(__VA_ARGS__) "\n" REST(__VA_ARGS__))

// ////
//https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick/11172679#11172679
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

#define C_TWENTIETH_ARG(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20, ...) a20
#define C_LOG_BLOCK(...) C_TWENTIETH_ARG(__VA_ARGS__, \
  C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_FORMAT(this, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(this, __VA_ARGS__), C_LOG_WITH_ONE_STRING(this, FIRST(__VA_ARGS__)), C_LOG_WITHOUT_FORMAT(this) )

#define C_LOG_BLOCK_NO_THIS(...) C_TWENTIETH_ARG(__VA_ARGS__, \
  C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), \
  C_LOG_WITH_FORMAT(nullptr, __VA_ARGS__), C_LOG_WITH_ONE_STRING(nullptr, FIRST(__VA_ARGS__)), C_LOG_WITHOUT_FORMAT(nullptr) )

#define C_LOG(...) \
  { \
    char blockScopeLogCustomBuffer[C_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, C_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.log(__LINE__, blockScopeLogCustomBuffer); \
  }

#define C_ERROR(...) \
  { \
    char blockScopeLogCustomBuffer[C_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, C_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.error(__LINE__, blockScopeLogCustomBuffer); \
  }

#define C_SET(name, value) \
  { \
    blockScopeLog.set(__LINE__, name, value); \
  }

class BlockLogger {
public:
  BlockLogger(const void* thisPointer);
  ~BlockLogger();

  void setPrimaryLog(int line, const char* logInfoBuffer, const char* customMessageBuffer);

  void log(int line, const char* messageBuffer);

  void error(int line, const char* messageBuffer);

  void set(int line, std::string name, std::string value);

private:
  char mlogInfoBuffer[C_LOG_BUFFER_SIZE];
  bool mhasCustomMessage = false;
  char mcustomMessageBuffer[C_LOG_BUFFER_SIZE];
  unsigned int mId;
  unsigned int mDepth;
  unsigned int mThreadId;
  const void* mThisPointer;
};