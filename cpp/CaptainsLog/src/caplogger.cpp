#define ENABLE_CAP_LOGGER
#ifdef ENABLE_CAP_LOGGER

#include <assert.h>
#include <array>
#include <chrono>
#include <iomanip>
#include <vector>

#include "caplogger.hpp"
#include "output.hpp"

namespace CAP {

namespace {

static const char* colourArray[] = {
        CAP_COLOUR CAP_BOLD CAP_RED,     CAP_COLOUR CAP_BOLD CAP_GREEN,
        CAP_COLOUR CAP_BOLD CAP_YELLOW,  CAP_COLOUR CAP_BOLD CAP_BLUE,
        CAP_COLOUR CAP_BOLD CAP_MAGENTA, CAP_COLOUR CAP_BOLD CAP_CYAN,
        CAP_COLOUR CAP_BOLD CAP_WHITE,
};
static const int colourArraySize = sizeof(colourArray) / sizeof(colourArray[0]);

struct PrintPrefix {
    unsigned int processId;
    unsigned int threadId;
    unsigned int channelId;
};

std::ostream& operator<<(std::ostream& os, const PrintPrefix& printPrefix) {
    os << CAP_COLOUR CAP_BOLD CAP_YELLOW << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
       << CAP_PROCESS_ID_DELIMITER << printPrefix.processId << " " << CAP_THREAD_ID_DELIMITER
       << colourArray[printPrefix.threadId % colourArraySize] << printPrefix.threadId
       << CAP_COLOUR CAP_BOLD CAP_GREEN << " " << CAP_CHANNEL_ID_DELIMITER << std::setw(3)
       << std::setfill('0') << printPrefix.channelId << " ";
    return os;
}

struct TabDelims {
    unsigned int depth;
};

std::ostream& operator<<(std::ostream& os, const TabDelims& tabDelims) {
    for (unsigned int i = 0; i < tabDelims.depth; ++i) {
        os << CAP_TAB_DELIMITER;
    }
    return os;
}

void writeOutput(const std::string& messageBuffer, unsigned int processId, unsigned int threadId,
                 unsigned int channelId, unsigned int depth) {
    // Note: newline characters are inconsistently required in different loggers, so we don't count
    // as part of the line length and instead just added a bit of padding to the max chars for the
    // cases where it's needed.
    std::stringstream completeOutputStream;
    completeOutputStream << PrintPrefix{processId, threadId, channelId} << TabDelims{depth}
                         << messageBuffer;
    auto outputStringSize = completeOutputStream.tellp();

    auto log_line_character_limit =
            CAP::OutputModeToLogLineCharLimit[static_cast<int>(CAP::DefaultOutputMode)];
    if (outputStringSize < log_line_character_limit) {
        completeOutputStream
                << CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
        PRINT_TO_LOG(completeOutputStream.str().c_str());
    } else {
        std::string completeOutputString = completeOutputStream.str();
        std::stringstream concatBeginStream;
        concatBeginStream << PrintPrefix{processId, threadId, channelId}
                          << CAP_CONCAT_DELIMITER_BEGIN;
        const std::string concatBeginString = concatBeginStream.str();
        size_t concatBeginLength = concatBeginString.size();
        size_t substrMax = log_line_character_limit -
                           concatBeginLength;  // TODO don't use 1, use size of newline.
        std::string currentLine =
                concatBeginString + completeOutputString.substr(0, substrMax) +
                CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
        size_t index = substrMax;
        PRINT_TO_LOG(currentLine.c_str());

        std::stringstream concatContinueStream;
        concatContinueStream << PrintPrefix{processId, threadId, channelId}
                             << CAP_CONCAT_DELIMITER_CONTINUE;
        const std::string concatContinueString = concatContinueStream.str();
        size_t concatContinueLength = concatContinueString.size();
        assert(concatContinueLength < log_line_character_limit);
        substrMax = log_line_character_limit - concatContinueLength;
        while (index < completeOutputString.size()) {
            std::string currentLine =
                    concatContinueString + completeOutputString.substr(index, substrMax) +
                    CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
            index += substrMax;
            PRINT_TO_LOG(currentLine.c_str());
        }

        std::stringstream concatEndStream;
        concatEndStream << PrintPrefix{processId, threadId, channelId} << CAP_CONCAT_DELIMITER_END
                        << +CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
        const std::string concatEndString = concatEndStream.str();
        PRINT_TO_LOG(concatEndString.c_str());
    }
}
}  // namespace

// NOTE: need to always have a default channel
BlockLogger::BlockLogger(const void* thisPointer, CAP::CHANNEL channel, std::string_view fileId,
                         std::string_view processId)
    : mEnabledMode(CAP::getChannelFlagMap()[(size_t)channel]),
      mlogInfoBuffer(),
      mcustomMessageBuffer(),
      mId(0),
      mDepth(0),
      mThreadId(0),
      mProcessId(0),
      mChannel(channel),
      mThisPointer(thisPointer) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        tlsScopeStack_ = &CAP::TLSScopeStack::getThreadLocalInstance();
        tlsScopeStack_->blocks.push(TLSScopeBlock{this, fileId, processId});

        // If this block is silent and can't write to output, no need to record
        // depth, id, etc which are used for printing to the log.
        auto logData = BlockLoggerDataStore::getInstance().newBlockLoggerInstance();
        mDepth = logData.logDepth;
        mId = logData.perThreadUniqueFunctionIdx;
        mThreadId = logData.relativeThreadIdx;
        mProcessId = logData.processTimestampInstanceKey;
    }
}

void BlockLogger::setPrimaryLog(int line, std::string_view logInfoBuffer,
                                std::string_view customMessageBuffer) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        mlogInfoBuffer = std::move(logInfoBuffer);

        if (mlogInfoBuffer.size() > CAP::LogAbsoluteCharacterLimitForUserLog) {
            mlogInfoBuffer.resize(CAP::LogAbsoluteCharacterLimitForUserLog);
        }

        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_PRIMARY_LOG_BEGIN_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer << " "
           << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize]
           << mThisPointer << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

        // The macro which calls this hardcodes a " " to get around some macro limitations regarding
        // zero/1/multi argument __VA_ARGS__
        if (customMessageBuffer.size() > 1) {
            log(line, std::move(customMessageBuffer));
        }
    }
}

void BlockLogger::dumpToFile(int line, std::string_view filename, const void* pointerToBuffer,
                             size_t numberOfBytes) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
           << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET
           << "] LOG: DUMP_TO_FILE | filename: [" << filename << "] | pointerToBuffer: ["
           << pointerToBuffer << "] | numberOfBytes: [" << numberOfBytes << "]";

        // TODO: use this after introducing file dump type
        // ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_FILEDUMP_DELIMITER <<
        // CAP_ADD_FILEDUMP_SECOND_DELIMITER << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " "
        // << CAP_COLOUR CAP_RESET << "[" << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR
        // CAP_RESET << "] DUMP_TO_FILE | filename: [" << filename
        // << "] | pointerToBuffer: [" << pointerToBuffer << "] | numberOfBytes: [" << numberOfBytes
        // << "]";

        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

        PRINT_TO_BINARY_FILE(filename, pointerToBuffer, numberOfBytes);
    }
}

void BlockLogger::log(int line, std::string_view messageBuffer) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
           << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET << "] LOG: ";

        if (messageBuffer.size() <= CAP::LogAbsoluteCharacterLimitForUserLog) {
            ss << messageBuffer;
        } else {
            ss << messageBuffer.substr(0, CAP::LogAbsoluteCharacterLimitForUserLog);
        }

        ss << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
    }
}

void BlockLogger::error(int line, std::string_view messageBuffer) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
           << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET << "] "
           << CAP_COLOUR CAP_BOLD CAP_RED << "ERROR: " << messageBuffer << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
    }
}

//-------- State Addresses
void BlockLogger::printState(int line, const DataStoreKey& key,
                             const DataStoreMemberVariableName& varName) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        auto states = BlockLoggerDataStore::getInstance().getStates(storeKeyList(key),
                                                                    variableNames(varName));
        printStateImpl(line, "PRINT STATE", to_string(key), varName, states[0]);
    }
}

void BlockLogger::printAllStateOfStore(int line, const DataStoreKey& key) {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        auto allStates = BlockLoggerDataStore::getInstance().getAllStates(key);

        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
           << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET << "] "
           << CAP_COLOUR CAP_BOLD CAP_RED << "PRINTING ALL STATE IN STORE: StoreKey='"
           << to_string(key) << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

        for (const auto& row : allStates) {
            printStateImpl(line, "PRINT STATE", to_string(key), row.first, row.second);
        }
    }
}

void BlockLogger::releaseState(int line, const DataStoreKey& key,
                               const DataStoreMemberVariableName& varName) {
    if (!(mEnabledMode)) {
        return;
    }

    std::optional<std::string> oldState =
            BlockLoggerDataStore::getInstance().releaseState(key, varName);

    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        printStateImpl(line, "RELEASE STATE", to_string(key), varName, oldState);
    }
}

void BlockLogger::releaseAllStateOfStore(int line, const DataStoreKey& key) {
    if (!(mEnabledMode)) {
        return;
    }

    int deletedCount = BlockLoggerDataStore::getInstance().releaseAllStates(key);

    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
           << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET << "] "
           << CAP_COLOUR CAP_BOLD CAP_RED << "RELEASE ALL STATE IN STORE: StoreKey='"
           << to_string(key) << "' NumDeleted='" << deletedCount << "'" << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
    }
}

void BlockLogger::printStateImpl(int line, const std::string& logCommand,
                                 const std::string& storeKey, const std::string& varName,
                                 const std::optional<std::string>& value) {
    std::stringstream ss;
    ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
       << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << " " << CAP_COLOUR CAP_RESET << "["
       << CAP_COLOUR CAP_BOLD CAP_GREEN << line << CAP_COLOUR CAP_RESET << "] "
       << CAP_COLOUR CAP_BOLD CAP_RED << logCommand << ": "
       << "StoreKey='" << storeKey << "' : StateName='" << varName << "' : Value='"
       << value.value_or("N/A") << "'" << CAP_COLOUR CAP_RESET;
    writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);
}

BlockLogger::~BlockLogger() {
    if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
        // block logger instance is only created when logging/output mode enabled.
        BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
        std::stringstream ss;
        ss << CAP_COLOUR CAP_BOLD CAP_GREEN << CAP_PRIMARY_LOG_END_DELIMITER
           << CAP_COLOUR CAP_BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer << " "
           << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize]
           << mThisPointer << CAP_COLOUR CAP_RESET;
        writeOutput(ss.str(), mProcessId, mThreadId, (size_t)mChannel, mDepth);

        if (tlsScopeStack_ != nullptr) {
            tlsScopeStack_->blocks.pop();
        }
    }
}

}  // namespace CAP

#endif