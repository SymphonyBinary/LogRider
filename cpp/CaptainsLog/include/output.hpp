#pragma once

#include "outputfile.hpp"
#include "outputsocket.hpp"
#include "outputstdout.hpp"
#include "utilities.hpp"

#define PRINT_TO_LOG(outputString) CAP::writeToOutput(CAP::DefaultOutputMode, outputString)
#define PRINT_TO_BINARY_FILE(filename, pointerToBuffer, numberOfBytes) \
    CAP::writeToBinaryFile(CAP::DefaultOutputMode, filename, pointerToBuffer, numberOfBytes);

// Reference for getting the default newline char/string:
//   CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
//
// Reference for getting the default log line limit
//   CAP::OutputModeToLogLineCharLimit[static_cast<int>(CAP::DefaultOutputMode)];

namespace CAP {

constexpr const int LogAbsoluteCharacterLimitForUserLog = 1000000;

#ifdef PIPE_BUF
constexpr const int pipe_size = PIPE_BUF;
#else
// int pipe_size = fcntl(pipe_des[1], F_SETPIPE_SZ, sizeof(size_t));
constexpr const int pipe_size = 4096;
#endif

// 1 - enum
// 3 - log_line_character_limit
// 3 - newline character
// 4 - function to alias for text output
#define OUTPUT_MODES                                                      \
    OUTPUT_MODE(StandardOut, 100000, "\n", writeToStandardOut)            \
    OUTPUT_MODE(Logcat, 150, "", writeToLogcat)                           \
    OUTPUT_MODE(File, pipe_size - 4, "\n", FileLogger::writeToOutputFile) \
    OUTPUT_MODE(Socket, 1000, "\n", SocketLogger::writeToSocket)          \
    OUTPUT_MODE(Noop, 100000, "", noop)

inline void noop(const std::string&) {}

enum class OutputMode {
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) name,
    OUTPUT_MODES
#undef OUTPUT_MODE
};

constexpr const char* OutputModeToString[] = {
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) #name,
        OUTPUT_MODES
#undef OUTPUT_MODE
};

// log_line_character_limit does not account for newline character, just the characters in the line.
// This is because some loggers require a newline (eg. printf) and some don't (eg. android logcat).
// So to be safe, we scale back our log_line_character_limit max a bit to account for an optional
// newline that we might need to insert.
constexpr const int OutputModeToLogLineCharLimit[] = {
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) \
    log_line_character_limit,
        OUTPUT_MODES
#undef OUTPUT_MODE
};

constexpr const char* OutputModeToNewLineChar[] = {
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) newline_character,
        OUTPUT_MODES
#undef OUTPUT_MODE
};

inline void writeToOutput(OutputMode mode, const std::string& output) {
    switch (mode) {
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) \
    case OutputMode::name:                                                       \
        function(output);                                                        \
        break;
        OUTPUT_MODES
#undef OUTPUT_MODE
    default:
        break;
    }
}

inline void writeToBinaryFile(OutputMode mode, std::string_view filename,
                              const void* pointerToBuffer, size_t numberOfBytes) {
    // only currently supported for socket output mode
    if (mode == OutputMode::Socket) {
        SocketLogger::writeBinaryStreamToSocket(filename, pointerToBuffer, numberOfBytes);
    }
}

// make sure that none of the log line limits are smaller than 100, of the'll format poorly.
#define OUTPUT_MODE(name, log_line_character_limit, newline_character, function) \
    static_assert(log_line_character_limit >= 100, "log_line_character_limit must be >= 100");
OUTPUT_MODES
#undef OUTPUT_MODE

#ifdef CAPLOG_SOCKET_ENABLED
constexpr const OutputMode DefaultOutputMode = OutputMode::Socket;
#else
constexpr const OutputMode DefaultOutputMode = OutputMode::StandardOut;
#endif

inline void printLogLineCharacterLimit(std::stringstream& ss, unsigned int processId) {
    ss << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
       << CAP_PROCESS_ID_DELIMITER << processId << " " << CAP_MAX_CHAR_SIZE_DELIMITER
       << CAP::OutputModeToLogLineCharLimit[static_cast<int>(CAP::DefaultOutputMode)]
       << CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
}

}  // namespace CAP
