#pragma once

#include <any>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stack>
#include <string_view>

#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <inttypes.h>

#include <assert.h>
#include <array>
#include <chrono>
#include <iomanip>
#include <vector>

#include "constants.hpp"
#include "datastore.hpp"
#include "utilities.hpp"
#include "outputsocket.hpp"

// #define CAP_LOG_STRING_TEMPLATE_CHANNEL
#define CAP_LOG_STRING_TEMPLATE_CHANNEL

#ifdef CAP_LOG_STRING_TEMPLATE_CHANNEL
#define CAP_LOG_DEFAULT_CHANNEL 0
// #define CAP_LOG_CHANNEL_ARG_TYPE std::string_view
// #define CAP_LOG_CHANNEL_ENABLED(channel) CAP::ChannelEnabledMode::FULLY_ENABLED
// #define CAP_LOG_CHANNEL_ENABLE_MODE(svChannel) Channel<as_sequence<svChannel>::type>::enableMode()
// #define CAP_LOG_CHANNEL_ID(svChannel) CAP::Channel<CAP::as_sequence<svChannel>::type>::id()
#else
#define CAP_LOG_DEFAULT_CHANNEL CAP::CHANNEL::DEFAULT
#define CAP_LOG_CHANNEL_ARG_TYPE CAP::CHANNEL
#define CAP_LOG_CHANNEL_ENABLE_MODE(channel) CAP::getChannelFlagMap()[(size_t)channel]
#endif

namespace CAP {

namespace Impl{

struct PrintPrefix {
    unsigned int processId;
    unsigned int threadId;
    unsigned int channelId;
};

std::ostream& operator<<(std::ostream& os, const PrintPrefix& printPrefix) {
    os << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
       << CAP_PROCESS_ID_DELIMITER << printPrefix.processId << " " << CAP_THREAD_ID_DELIMITER
       << printPrefix.threadId << " " << CAP_CHANNEL_ID_DELIMITER << std::setw(3)
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
}  // namespace Impl

struct TLSScopeBlock {
    class BlockLogger* blockScope = nullptr;
    std::string_view blockScopeFileId;
    std::string_view blockScopeFunctionId;
};

struct TLSScopeStack {
    static TLSScopeStack& getThreadLocalInstance() {
        thread_local TLSScopeStack scopeStack{};
        return scopeStack;
    }

    std::stack<TLSScopeBlock> blocks{};
};

// 1 - get the current scope
// 2 - check if the current scope has the same file/function as the requestor
// 3 - if it is, a scope already exists with the file/function, so use that scope
// 4 - if it isn't, we need to create a new scope and push it on the stack
struct TLSScope {
    TLSScope(std::string_view fileId, std::string_view functionId) {
        TLSScopeStack* tlsScopeStack = &CAP::TLSScopeStack::getThreadLocalInstance();

        if (!tlsScopeStack->blocks.empty()) {
            const auto& topBlock = tlsScopeStack->blocks.top();
            if (topBlock.blockScopeFileId == fileId &&
                topBlock.blockScopeFunctionId == functionId) {
                blockLog = topBlock.blockScope;
            }
        }

        if (!blockLog) {
            // anonymousBlockLog = std::make_unique<BlockLogger>(nullptr, 0,
            //                                                   fileId, functionId);
            anonymousBlockLog = std::make_unique<BlockLogger>(nullptr, CAP_LOG_DEFAULT_CHANNEL, true,
                                                              fileId, functionId);
            blockLog = anonymousBlockLog.get();
            // TODO: set a flag in the anonymous block so we can specify unknown depth.
        }
    }

    std::unique_ptr<BlockLogger> anonymousBlockLog = nullptr;
    BlockLogger* blockLog = nullptr;
};

class BlockLogger {
  public:
    // NOTE: need to always have a default channel
    BlockLogger(const void* thisPointer, size_t channelId, uint32_t enabledMode, std::string_view fileId,
                std::string_view processId) 
            : mEnabledMode(enabledMode),
            mlogInfoBuffer(),
            mcustomMessageBuffer(),
            mId(0),
            mDepth(0),
            mThreadId(0),
            mProcessId(0),
            mChannel(channelId),
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
                
    ~BlockLogger() {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            // block logger instance is only created when logging/output mode enabled.
            BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
            std::stringstream ss;
            ss << CAP_PRIMARY_LOG_END_DELIMITER << " " << mId 
               << mlogInfoBuffer << " " << mThisPointer;
            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);

            if (tlsScopeStack_ != nullptr) {
                tlsScopeStack_->blocks.pop();
            }
        }
    }

    void setPrimaryLog(int line, std::string_view logInfoBuffer,
                       std::string_view customMessageBuffer) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            mlogInfoBuffer = std::move(logInfoBuffer);

            if (mlogInfoBuffer.size() > CAP::LogAbsoluteCharacterLimitForUserLog) {
                mlogInfoBuffer.resize(CAP::LogAbsoluteCharacterLimitForUserLog);
            }

            std::stringstream ss;
            ss << CAP_PRIMARY_LOG_BEGIN_DELIMITER
            << " " << mId << mlogInfoBuffer << " "
            << mThisPointer;
            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);

            // The macro which calls this hardcodes a " " to get around some macro limitations regarding
            // zero/1/multi argument __VA_ARGS__
            if (customMessageBuffer.size() > 1) {
                log(line, std::move(customMessageBuffer));
            }
        }
    }

    void dumpToFile(int line, std::string_view filename, const void* pointerToBuffer,
                    size_t numberOfBytes) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            std::stringstream ss;
            ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
            << " " << mId << " " << "[" << line
            << "] LOG: DUMP_TO_FILE | filename: [" << filename << "] | pointerToBuffer: ["
            << pointerToBuffer << "] | numberOfBytes: [" << numberOfBytes << "]";

            // TODO: use this after introducing file dump type
            // ss << CAP_ADD_FILEDUMP_DELIMITER <<
            // CAP_ADD_FILEDUMP_SECOND_DELIMITER << " " << mId << " "
            // << "[" << line << "] DUMP_TO_FILE | filename: [" << filename
            // << "] | pointerToBuffer: [" << pointerToBuffer << "] | numberOfBytes: [" << numberOfBytes
            // << "]";

            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);

            PRINT_TO_BINARY_FILE(filename, pointerToBuffer, numberOfBytes);
        }
    }

    void log(int line, std::string_view messageBuffer) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            std::stringstream ss;
            ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
            << " " << mId << " " << "["
            << line << "] LOG: ";

            if (messageBuffer.size() <= CAP::LogAbsoluteCharacterLimitForUserLog) {
                ss << messageBuffer;
            } else {
                ss << messageBuffer.substr(0, CAP::LogAbsoluteCharacterLimitForUserLog);
            }

            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);
        }
    }

    void error(int line, std::string_view messageBuffer) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            std::stringstream ss;
            ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
            << " " << mId << " " << "["
            << line << "] " << "ERROR: " << messageBuffer;
            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);
        }
    }

    // UpdaterFunc is a callable that receives DataStoreStateArray<DATA_COUNT>&
    // as its only input parameter.  Return is unused/ignored.
    template <size_t DATA_COUNT, class UpdaterFunc>
    void updateState(int line, const DataStoreKeysArrayN<DATA_COUNT>& keys,
                     const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& varNames,
                     const UpdaterFunc& stateUpdater) {
        if (!mEnabledMode) {
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

    void printState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            auto states = BlockLoggerDataStore::getInstance().getStates(storeKeyList(key),
                                                                        variableNames(varName));
            printStateImpl(line, "PRINT STATE", to_string(key), varName, states[0]);
        }
    }

    void printAllStateOfStore(int line, const DataStoreKey& key) {
        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            auto allStates = BlockLoggerDataStore::getInstance().getAllStates(key);

            std::stringstream ss;
            ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
            << " " << mId << " ["
            << line << "] "
            << "PRINTING ALL STATE IN STORE: StoreKey='"
            << to_string(key);
            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);

            for (const auto& row : allStates) {
                printStateImpl(line, "PRINT STATE", to_string(key), row.first, row.second);
            }
        }
    }

    void releaseState(int line, const DataStoreKey& key, const std::string& varName) {
        if (!(mEnabledMode)) {
            return;
        }

        std::optional<std::string> oldState =
                BlockLoggerDataStore::getInstance().releaseState(key, varName);

        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            printStateImpl(line, "RELEASE STATE", to_string(key), varName, oldState);
        }
    }

    void releaseAllStateOfStore(int line, const DataStoreKey& key) {
        if (!(mEnabledMode)) {
            return;
        }

        int deletedCount = BlockLoggerDataStore::getInstance().releaseAllStates(key);

        if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
            std::stringstream ss;
            ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
            << " " << mId << " ["
            << line << "] "
            << "RELEASE ALL STATE IN STORE: StoreKey='"
            << to_string(key) << "' NumDeleted='" << deletedCount << "'";
            Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);
        }
    }

  private:
    void printStateImpl(int line, const std::string& logCommand, const std::string& storeKey,
                        const std::string& varName, const std::optional<std::string>& value) {
                            std::stringstream ss;
        ss << CAP_ADD_LOG_DELIMITER << CAP_ADD_LOG_SECOND_DELIMITER
           << " " << mId << " [" << line << "] " << logCommand << ": "
           << "StoreKey='" << storeKey << "' : StateName='" << varName << "' : Value='"
           << value.value_or("N/A") << "'";
        Impl::writeOutput(ss.str(), mProcessId, mThreadId, mChannel, mDepth);
    }

    TLSScopeStack* tlsScopeStack_ = nullptr;
    std::string_view fileId_;
    std::string_view functionId_;

    const uint32_t mEnabledMode = FULLY_DISABLED;
    std::string mlogInfoBuffer;
    std::string mcustomMessageBuffer;
    unsigned int mId;
    unsigned int mDepth;
    unsigned int mThreadId;
    unsigned int mProcessId;
    size_t mChannel;
    const void* mThisPointer;
}; 

}  // namespace CAP
