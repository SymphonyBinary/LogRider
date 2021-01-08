#include <iostream>
#include <cstdlib>

#include <thread>
#include <cstring>

//https://stackoverflow.com/questions/45526532/c-xcode-how-to-output-color

#define COLOURIZE

#ifdef COLOURIZE

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#else

#define RESET ""
#define BLACK ""
#define RED ""
#define GREEN ""
#define YELLOW ""
#define BLUE ""
#define MAGENTA ""
#define CYAN ""
#define WHITE ""
#define BOLDBLACK ""
#define BOLDRED ""
#define BOLDGREEN ""
#define BOLDYELLOW ""
#define BOLDBLUE ""
#define BOLDMAGENTA ""
#define BOLDCYAN ""
#define BOLDWHITE ""

#endif

// ╔ Unicode: U+2554, UTF-8: E2 95 94
#define BOX_DRAWINGS_DOUBLE_DOWN_AND_RIGHT "\u2554"
// ╠ Unicode: U+2560, UTF-8: E2 95 A0
#define BOX_DRAWINGS_DOUBLE_VERTICAL_AND_RIGHT "\u2560"
// ╾ Unicode: U+257E, UTF-8: E2 95 BE
#define BOX_DRAWINGS_HEAVY_LEFT_AND_LIGHT_RIGHT "\u257E"
// ╚ Unicode: U+255A, UTF-8: E2 95 9A
#define BOX_DRAWINGS_DOUBLE_UP_AND_RIGHT "\u255A"
// … Unicode: U+2026, UTF-8: E2 80 A6
#define HORIZONTAL_ELLIPSIS "\u2026"
// ║ Unicode: U+2551, UTF-8: E2 95 91
#define BOX_DRAWINGS_DOUBLE_VERTICAL "\u2551"

//https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define D_LOG_BUFFER_SIZE 200

//change to __PRETTY_FUNCTION__ if you want the whole function signature
#define D_LOG_WITHOUT_FORMAT() \
  BlockLogger blockScopeLog; \
  { \
    char blockScopeLogInfoBuffer[D_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogInfoBuffer, D_LOG_BUFFER_SIZE, BOLDCYAN " [%d] - " BOLDMAGENTA "[%s]::[%s]", __LINE__, __FILENAME__, __FUNCTION__); \
    blockScopeLog.setPrimaryLog(__LINE__, blockScopeLogInfoBuffer, nullptr); \
  } \

#define D_LOG_WITH_FORMAT(...) \
  BlockLogger blockScopeLog; \
  { \
    char blockScopeLogInfoBuffer[D_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogInfoBuffer, D_LOG_BUFFER_SIZE, BOLDCYAN " [%d] - " BOLDMAGENTA "[%s]::[%s]", __LINE__, __FILENAME__, __FUNCTION__); \
    char blockScopeLogCustomBuffer[D_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, D_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.setPrimaryLog(__LINE__, blockScopeLogInfoBuffer, blockScopeLogCustomBuffer); \
  }

#define D_TWENTIETH_ARG(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20, ...) a20
#define D_LOG_BLOCK(...) D_TWENTIETH_ARG(dummy, ## __VA_ARGS__, \
  D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), \
  D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), \
  D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), \
  D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), \
  D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITH_FORMAT(__VA_ARGS__), D_LOG_WITHOUT_FORMAT() )

#define D_LOG(...) \
  { \
    char blockScopeLogCustomBuffer[D_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, D_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.log(__LINE__, blockScopeLogCustomBuffer); \
  }

#define D_ERROR(...) \
  { \
    char blockScopeLogCustomBuffer[D_LOG_BUFFER_SIZE]; \
    snprintf(blockScopeLogCustomBuffer, D_LOG_BUFFER_SIZE, __VA_ARGS__); \
    blockScopeLog.error(__LINE__, blockScopeLogCustomBuffer); \
  }

thread_local unsigned int gBlockLoggerDepth = 0;

class BlockLogger {
public:
  BlockLogger() : mDepth(gBlockLoggerDepth++) {}
    
  void setPrimaryLog(int line, const char* logInfoBuffer, const char* customMessageBuffer) {
    mhasCustomMessage = (customMessageBuffer != nullptr);
      
    thread_local unsigned int unique_index_counter = 0;
    mId = unique_index_counter++;
    
    std::memcpy(&mlogInfoBuffer, logInfoBuffer, D_LOG_BUFFER_SIZE);
    printTab();

    std::cout << BOLDGREEN << BOX_DRAWINGS_DOUBLE_DOWN_AND_RIGHT << BOLDYELLOW << " " << mId << mlogInfoBuffer << std::endl;

    if(mhasCustomMessage) {
      std::memcpy(&mcustomMessageBuffer, customMessageBuffer, D_LOG_BUFFER_SIZE);
      log(line, mcustomMessageBuffer);
    }
  }
    
  void log(int line, const char* messageBuffer) {
    printTab();
    std::cout << BOLDGREEN << BOX_DRAWINGS_DOUBLE_VERTICAL_AND_RIGHT << BOX_DRAWINGS_HEAVY_LEFT_AND_LIGHT_RIGHT << BOLDCYAN << "[" << line << "] " << RESET << messageBuffer << std::endl;
  }

  void error(int line, const char* messageBuffer) {
    printTab();
    std::cout << BOLDGREEN << BOX_DRAWINGS_DOUBLE_VERTICAL_AND_RIGHT << BOX_DRAWINGS_HEAVY_LEFT_AND_LIGHT_RIGHT << BOLDCYAN << "[" << line << "] " << BOLDRED << messageBuffer << RESET <<std::endl;
  }
    
  ~BlockLogger() {
    --gBlockLoggerDepth;
    printTab();
    std::cout << BOLDGREEN << BOX_DRAWINGS_DOUBLE_UP_AND_RIGHT << BOLDYELLOW << " " << mId << mlogInfoBuffer << std::endl;
  }
  
private:
  void printTab() {

    std::cout << BOLDYELLOW << HORIZONTAL_ELLIPSIS << std::this_thread::get_id() << BOLDGREEN << " ";
    for(unsigned int i = 0 ; i < mDepth; ++i ){
      std::cout << BOX_DRAWINGS_DOUBLE_VERTICAL;
    }
  }

  char mlogInfoBuffer[D_LOG_BUFFER_SIZE];
  bool mhasCustomMessage = false;
  char mcustomMessageBuffer[D_LOG_BUFFER_SIZE];
  unsigned int mId;
  unsigned int mDepth;
};

