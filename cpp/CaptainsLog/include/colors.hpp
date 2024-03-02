
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
  #define CAP_BLACK "30m"
  #define CAP_RED "31m"
  #define CAP_GREEN "32m"
  #define CAP_YELLOW  "33m"
  #define CAP_BLUE    "34m"
  #define CAP_MAGENTA "35m"
  #define CAP_CYAN    "36m"
  #define CAP_WHITE   "37m"
#else
  #define COLOUR ""
  #define BOLD ""
  #define RESET ""
  #define CAP_BLACK ""
  #define CAP_RED ""
  #define CAP_GREEN ""
  #define CAP_YELLOW  ""
  #define CAP_BLUE    ""
  #define CAP_MAGENTA ""
  #define CAP_CYAN    ""
  #define CAP_WHITE   ""
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
  #define MAIN_PREFIX_DELIMITER "CAP_LOG"
  #define PROCESS_ID_DELIMITER "P="
  #define THREAD_ID_DELIMITER "T="
  #define TAB_DELIMITER ":"
#endif
