#pragma once

#ifndef ENABLE_CAP_LOGGER

#define CAP_LOG_BLOCK(...)
#define CAP_LOG_BLOCK_NO_THIS(...)

#define CAP_LOG(...)
#define CAP_LOG_ERROR(...)

#define CAP_LOG_ANONYMOUS(...)
#define CAP_LOG_ERROR_ANONYMOUS(...)

#define CAP_LOG_UPDATE_STATE_ON(...)
#define CAP_LOG_PRINT_STATE_ON(...)
#define CAP_LOG_PRINT_ALL_STATE_ON(...)
#define CAP_LOG_RELEASE_STATE_ON(...)
#define CAP_LOG_RELEASE_ALL_STATE_ON(...)

#define CAP_LOG_UPDATE_STATE(...)
#define CAP_LOG_PRINT_STATE(...)
#define CAP_LOG_PRINT_ALL_STATE(...)
#define CAP_LOG_RELEASE_STATE(...)
#define CAP_LOG_RELEASE_ALL_STATE(...)

#define CAP_LOG_EXECUTE_LAMBDA(...)

#define CAP_LOG_DECLARE_ANY_VAR(...)

#define CAP_LOG_ON_FORK(...)

#define CAP_SCAN_BLOCK_NO_THIS(...)
#define CAP_SCAN_BLOCK(...)
#define CAP_SCAN(...)

#define CAP_DUMP_TO_FILE(...)

#else

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

#include "capdata.hpp"
#include "channels.hpp"
#include "utilities.hpp"

#include "outputsocket.hpp"

#define CAP_ESCAPE_COMMA ,

#if defined(__clang__)
#define PRAGMA_IGNORE_SHADOW_BEGIN \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wshadow\"")
#define PRAGMA_IGNORE_SHADOW_END _Pragma("clang diagnostic pop")
#else
#define PRAGMA_IGNORE_SHADOW_BEGIN
#define PRAGMA_IGNORE_SHADOW_END
#endif

/*
The " " here
snprintf(blockScopeLogCustomBuffer, CAP_LOG_BUFFER_SIZE, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__));
\ is because you will get warnings if you try to pass a zero sized string to sprintf. Ideally, you
want to branch if __VA_ARGS__ is empty and call setPrimaryLog without passing it a buffer at all
*/
#define CAP_LOG_INTERNAL(pointer, channel, ...)                                               \
    PRAGMA_IGNORE_SHADOW_BEGIN                                                                \
    [[maybe_unused]] constexpr bool channelCompileNotDisabled =                               \
            CAP::getChannelFlagMap()[(size_t)channel];                                        \
    [[maybe_unused]] constexpr bool channelCompileEnabledOutput =                             \
            CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_OUTPUT;             \
    [[maybe_unused]] constexpr bool channelCompileEnabledState =                              \
            CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_STATE;              \
    CAP::BlockLogger blockScopeLog{pointer, channel, __FILENAME__, __PRETTY_FUNCTION__};      \
    CAP::BlockLogger* blockScope = &blockScopeLog;                                            \
    PRAGMA_IGNORE_SHADOW_END                                                                  \
    if constexpr (channelCompileEnabledOutput) {                                              \
        std::stringstream CAPLOG_ss;                                                          \
        CAPLOG_ss << CAP_COLOUR CAP_RESET " [" CAP_COLOUR CAP_BOLD CAP_GREEN << __LINE__      \
                  << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_CYAN << __FILENAME__ \
                  << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_MAGENTA              \
                  << __PRETTY_FUNCTION__ << CAP_COLOUR CAP_RESET "]";                         \
        blockScope->setPrimaryLog(__LINE__, CAPLOG_ss.str(), "");                             \
        CAP_LOG(__VA_ARGS__);                                                                 \
    } else if constexpr (channelCompileEnabledState) {                                        \
        blockScope->setPrimaryLog(__LINE__, "", "");                                          \
    }

/// you may optionally provide an argument in the form of "(format, ...)"
#define CAP_LOG_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__)

#define CAP_LOG_CHANNEL_BLOCK(...) CAP_LOG_INTERNAL(this, __VA_ARGS__)
#define CAP_LOG_CHANNEL_BLOCK_NO_THIS(...) CAP_LOG_INTERNAL(nullptr, __VA_ARGS__)

// Need to rename this to "cap log to channel"
#define CAP_LOG(...)                             \
    if constexpr (channelCompileEnabledOutput) { \
        CAP_LOG_IMPL(__VA_ARGS__);               \
    }

#define CAP_LOG_IMPL(...)                                                            \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    if (needed > 2) {                                                                \
        char* CAPLOG_buffer = new char[needed];                                      \
        snprintf(CAPLOG_buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__));   \
        blockScope->log(__LINE__, CAPLOG_buffer);                                    \
        delete[] CAPLOG_buffer;                                                      \
    }

#define CAP_LOG_ERROR(...)                       \
    if constexpr (channelCompileEnabledOutput) { \
        CAP_LOG_ERROR_IMPL(__VA_ARGS__);         \
    }

#define CAP_LOG_ERROR_IMPL(...)                                                      \
    size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
    if (needed > 2) {                                                                \
        char* CAPLOG_buffer = new char[needed];                                      \
        snprintf(CAPLOG_buffer, needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__));   \
        blockScope->error(__LINE__, CAPLOG_buffer);                                  \
        delete[] CAPLOG_buffer;                                                      \
    }

// this emits a log and will use the current scope if possible.
// otherwise, creates an anonymous scope.
// Another option to to forward a macro in here like
// CAP_ANONY(channel, CAP_IMPL_MACRO, ...)
// but that can resolve into bad stuff pretty easily.
#define CAP_LOG_ANONYMOUS(channel, ...)                                                           \
    if constexpr (CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_OUTPUT) {         \
        CAP::TLSScope tlsScope(__FILENAME__, __PRETTY_FUNCTION__);                                \
        if (tlsScope.anonymousBlockLog != nullptr) {                                              \
            std::stringstream CAPLOG_ss;                                                          \
            CAPLOG_ss << CAP_COLOUR CAP_RESET " [" CAP_COLOUR CAP_BOLD CAP_GREEN << __LINE__      \
                      << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_CYAN << __FILENAME__ \
                      << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_MAGENTA              \
                      << __PRETTY_FUNCTION__ << CAP_COLOUR CAP_RESET "]";                         \
            tlsScope.anonymousBlockLog->setPrimaryLog(__LINE__, CAPLOG_ss.str(), "");             \
        }                                                                                         \
        PRAGMA_IGNORE_SHADOW_BEGIN                                                                \
        CAP::BlockLogger* blockScope = tlsScope.blockLog;                                         \
        PRAGMA_IGNORE_SHADOW_END                                                                  \
        CAP_LOG_IMPL(__VA_ARGS__);                                                                \
    }

#define CAP_LOG_ERROR_ANONYMOUS(channel, ...)                                                     \
    if constexpr (CAP::getChannelFlagMap()[(size_t)channel] & CAP::CAN_WRITE_TO_OUTPUT) {         \
        CAP::TLSScope tlsScope(__FILENAME__, __PRETTY_FUNCTION__);                                \
        if (tlsScope.anonymousBlockLog != nullptr) {                                              \
            std::stringstream CAPLOG_ss;                                                          \
            CAPLOG_ss << CAP_COLOUR CAP_RESET " [" CAP_COLOUR CAP_BOLD CAP_GREEN << __LINE__      \
                      << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_CYAN << __FILENAME__ \
                      << CAP_COLOUR CAP_RESET "]::[" CAP_COLOUR CAP_BOLD CAP_MAGENTA              \
                      << __PRETTY_FUNCTION__ << CAP_COLOUR CAP_RESET "]";                         \
            tlsScope.anonymousBlockLog->setPrimaryLog(__LINE__, CAPLOG_ss.str(), "");             \
        }                                                                                         \
        PRAGMA_IGNORE_SHADOW_BEGIN                                                                \
        CAP::BlockLogger* blockScope = tlsScope.blockLog;                                         \
        PRAGMA_IGNORE_SHADOW_END                                                                  \
        CAP_LOG_ERROR_IMPL(__VA_ARGS__);                                                          \
    }

// https://stackoverflow.com/questions/36030589/i-cannot-pass-lambda-as-stdfunction
// updaterLambda is of form std::function<std::string(std::optional<std::string>)>;
#define CAP_LOG_UPDATE_STATE_ON(storeKeys, variableNames, updaterLambda)                         \
    if constexpr (channelCompileEnabledState) {                                                  \
        CAP::DataStoreValuesArrayUpdater<decltype(storeKeys)::Size> updaterFunc = updaterLambda; \
        blockScope->updateState(__LINE__, storeKeys, variableNames, updaterFunc);                \
    }

#define CAP_LOG_PRINT_STATE_ON(storeKey, name)            \
    if constexpr (channelCompileEnabledOutput) {          \
        blockScope->printState(__LINE__, storeKey, name); \
    }

#define CAP_LOG_PRINT_ALL_STATE_ON(storeKey)                  \
    if constexpr (channelCompileEnabledOutput) {              \
        blockScope->printAllStateOfStore(__LINE__, storeKey); \
    }

#define CAP_LOG_RELEASE_STATE_ON(storeKey, name)            \
    if constexpr (channelCompileEnabledState) {             \
        blockScope->releaseState(__LINE__, storeKey, name); \
    }

#define CAP_LOG_RELEASE_ALL_STATE_ON(storeKey)                  \
    if constexpr (channelCompileEnabledState) {                 \
        blockScope->releaseAllStateOfStore(__LINE__, storeKey); \
    }

// WARNING: this must be called immediately after forking, and
// either all other threads must be closed before forking
// or all other threads must not execute any caplog decorated code.
// Similarly, this would need to be called on libs that are held behind shared libs.
#define CAP_LOG_ON_FORK()                                                 \
    {                                                                     \
        auto& loggerDataStore = CAP::BlockLoggerDataStore::getInstance(); \
        CAP::SocketLogger::getSocketLogger().reset();                     \
        loggerDataStore.onChildFork();                                    \
    }

#define CAP_LOG_UPDATE_STATE(name, updaterLambda) \
    CAP_LOG_UPDATE_STATE_ON(CAP::storeKeyList(this), CAP::variableNames(name), updaterLambda)
#define CAP_LOG_PRINT_STATE(name) CAP_LOG_PRINT_STATE_ON(this, name)
#define CAP_LOG_PRINT_ALL_STATE(name) CAP_LOG_PRINT_ALL_STATE_ON(this)
#define CAP_LOG_RELEASE_STATE(name) CAP_LOG_RELEASE_STATE_ON(this, name)
#define CAP_LOG_RELEASE_ALL_STATE() CAP_LOG_RELEASE_ALL_STATE_ON(this)

// "..." is because lambda capture often needs commas.  "CAP_LOG_EXECUTE_LAMBDA(lambda)" would not
// allow "lambda" to have commas.
// https://stackoverflow.com/questions/38030048/too-many-arguments-provided-to-function-like-macro-invocation
#define CAP_LOG_EXECUTE_LAMBDA(...)               \
    if constexpr (channelCompileNotDisabled) {    \
        std::function<void()> func = __VA_ARGS__; \
        func();                                   \
    }

#define CAP_LOG_DECLARE_ANY_VAR(channel, varName, varTypeIfEnabled, varInitIfEnabled,       \
                                varTypeIfDisabled, varInitIfDisabled)                       \
    using varName##_type = std::conditional<CAP::getChannelFlagMap()[(size_t)channel] != 0, \
                                            varTypeIfEnabled, varTypeIfDisabled>::type;     \
    std::any varName =                                                                      \
            CAP::getChannelFlagMap()[(size_t)channel] != 0 ? varInitIfEnabled : varInitIfDisabled;

#define CAP_SCAN_BLOCK_NO_THIS(...) CAP_LOG_BLOCK_NO_THIS(__VA_ARGS__)
#define CAP_SCAN_BLOCK(...) CAP_LOG_BLOCK(__VA_ARGS__)
#define CAP_SCAN(...) CAP_LOG(__VA_ARGS__)

#define CAP_DUMP_TO_FILE(filename, pointerToBuffer, numberOfBytes)                  \
    if constexpr (channelCompileEnabledOutput) {                                    \
        blockScope->dumpToFile(__LINE__, filename, pointerToBuffer, numberOfBytes); \
    }

namespace CAP {

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
            anonymousBlockLog = std::make_unique<BlockLogger>(nullptr, CAP::CHANNEL::DEFAULT,
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
    BlockLogger(const void* thisPointer, CAP::CHANNEL channel, std::string_view fileId,
                std::string_view processId);
    ~BlockLogger();

    void setPrimaryLog(int line, std::string_view logInfoBuffer,
                       std::string_view customMessageBuffer);

    void dumpToFile(int line, std::string_view filename, const void* pointerToBuffer,
                    size_t numberOfBytes);

    void log(int line, std::string_view messageBuffer);

    void error(int line, std::string_view messageBuffer);

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

    void printState(int line, const DataStoreKey& key, const DataStoreMemberVariableName& varName);
    void printAllStateOfStore(int line, const DataStoreKey& key);

    void releaseState(int line, const DataStoreKey& keys, const std::string& stateName);
    void releaseAllStateOfStore(int line, const DataStoreKey& key);

  private:
    void printStateImpl(int line, const std::string& logCommand, const std::string& storeKey,
                        const std::string& varName, const std::optional<std::string>& value);

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
    CAP::CHANNEL mChannel;
    const void* mThisPointer;
};

}  // namespace CAP

#endif