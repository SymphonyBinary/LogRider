#pragma once

#ifdef ENABLE_CAP_LOGGER

//#define CAP_COLOURIZE
//#define CAP_FANCY_ASCII
//#define SHOW_THREAD_ID
//#define ANDROID //defined outside of this, just here for the comment

// uses ANSI colours https://www.lihaoyi.com/post/BuildyourownCommandLinewithANSIescapecodes.html
#ifdef CAP_COLOURIZE
// https://stackoverflow.com/questions/45526532/c-xcode-how-to-output-color
#define CAP_COLOUR "\033["
//#define CAP_COLOUR= "\u001b[";
#define CAP_BOLD "1;"
#define CAP_RESET "0m"
#define CAP_BLACK "30m"
#define CAP_RED "31m"
#define CAP_GREEN "32m"
#define CAP_YELLOW "33m"
#define CAP_BLUE "34m"
#define CAP_MAGENTA "35m"
#define CAP_CYAN "36m"
#define CAP_WHITE "37m"
#else
#define CAP_COLOUR ""
#define CAP_BOLD ""
#define CAP_RESET ""
#define CAP_BLACK ""
#define CAP_RED ""
#define CAP_GREEN ""
#define CAP_YELLOW ""
#define CAP_BLUE ""
#define CAP_MAGENTA ""
#define CAP_CYAN ""
#define CAP_WHITE ""
#endif

#ifdef CAP_FANCY_ASCII
// ╔ Unicode: U+2554, UTF-8: E2 95 94
#define CAP_PRIMARY_LOG_BEGIN_DELIMITER "\u2554"
// ╠ Unicode: U+2560, UTF-8: E2 95 A0
#define CAP_ADD_LOG_DELIMITER "\u2560"
// ╾ Unicode: U+257E, UTF-8: E2 95 BE
#define CAP_ADD_LOG_SECOND_DELIMITER "\u257E"
// ╚ Unicode: U+255A, UTF-8: E2 95 9A
#define CAP_PRIMARY_LOG_END_DELIMITER "\u255A"
// … Unicode: U+2026, UTF-8: E2 80 A6
//#define CAP_MAIN_PREFIX_DELIMITER "\u2026"
#define CAP_MAIN_PREFIX_DELIMITER ""
#define CAP_PROCESS_ID_DELIMITER "P="
#define CAP_THREAD_ID_DELIMITER "T="
#define CAP_CHANNEL_ID_DELIMITER "C="
#define CAP_MAX_CHAR_SIZE_DELIMITER "MAX-CHAR-SIZE="
#define CAP_CONCAT_DELIMITER_BEGIN "|+ "
#define CAP_CONCAT_DELIMITER_CONTINUE "++ "
#define CAP_CONCAT_DELIMITER_END "+| "
// ║ Unicode: U+2551, UTF-8: E2 95 91
#define CAP_TAB_DELIMITER "\u2551"
#else
#define CAP_PRIMARY_LOG_BEGIN_DELIMITER "F"
#define CAP_ADD_LOG_DELIMITER "-"
#define CAP_ADD_LOG_SECOND_DELIMITER ">"
#define CAP_ADD_FILEDUMP_DELIMITER "-"
#define CAP_ADD_FILEDUMP_SECOND_DELIMITER "D"
#define CAP_PRIMARY_LOG_END_DELIMITER "L"
#define CAP_MAIN_PREFIX_DELIMITER "CAP_LOG"
#define CAP_PROCESS_ID_DELIMITER "P="
#define CAP_THREAD_ID_DELIMITER "T="
#define CAP_CHANNEL_ID_DELIMITER "C="
#define CAP_MAX_CHAR_SIZE_DELIMITER "MAX-CHAR-SIZE="
#define CAP_CONCAT_DELIMITER_BEGIN "|+ "
#define CAP_CONCAT_DELIMITER_CONTINUE "++ "
#define CAP_CONCAT_DELIMITER_END "+| END"
#define CAP_TAB_DELIMITER ":"
#endif

#endif