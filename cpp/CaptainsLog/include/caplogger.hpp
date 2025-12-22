#pragma once

#include "configdefines.hpp"

/*
Recommended approach to disabling caplog is to only unset ENABLE_CAP_LOGGER_IMPL
when building performance-optimized release builds.

For debug builds, the recommended approach is instead to define a top level 
channel that all your channels are children of.  Set that channel to 
enabled/disabled.  This will then ensure that the channels remain compilable,
but prevent any channel from executing any logic other than the creation of an
blocklogger on the stack.

NOTE: The BlockLogger object will avoid allocating from the heap during its constructor
only if the std::string implementation uses Small String Optimization.  If the compiler
doesn't use small string optimization, the options are to either implement SSO and
replace BlockLogger's std::string with that sso enabled string, or allocate the space
required to fit a blocklogger outside the scope brackets, and then if the constexpr
passes which enables the channel, do a placement-new of the blocklogger into that 
memory.

Most major compilers' implement std::string with SSO nowadays
*/

#ifdef ENABLE_CAP_LOGGER_IMPL

#include "blocklogger.hpp"
#include "channels.hpp"

#define CAP_LOGGER_ONLY

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
#define CAP_LOG_INTERNAL(pointer, channel, ...)                                        \
  PRAGMA_IGNORE_SHADOW_BEGIN \
  [[maybe_unused]] constexpr bool channelCompileNotDisabled = CAP_CHANNEL(channel)::enableMode(); \
  [[maybe_unused]] constexpr bool channelCompileEnabledOutput = CAP_CHANNEL(channel)::enableMode() & CAP::CAN_WRITE_TO_OUTPUT; \
  [[maybe_unused]] constexpr bool channelCompileEnabledState = CAP_CHANNEL(channel)::enableMode() & CAP::CAN_WRITE_TO_STATE; \
  [[maybe_unused]] const size_t channelId = CAP_CHANNEL(channel)::id(); \
  CAP::BlockLogger blockScopeLog = channelCompileNotDisabled ? \
    CAP::BlockLogger{} : \
    CAP::BlockLogger{pointer, channelId, CAP_CHANNEL_OUTPUT_MODE(channel), __FILENAME__, __PRETTY_FUNCTION__}; \
  CAP::BlockLogger* blockScope = &blockScopeLog; \
  PRAGMA_IGNORE_SHADOW_END                                                                  \
    if constexpr (channelCompileEnabledOutput) {                                              \
        std::stringstream CAPLOG_ss;                                                          \
        CAPLOG_ss << " [" << __LINE__                                                         \
                  << "]::[" << __FILENAME__                                                   \
                  << "]::[" << __PRETTY_FUNCTION__ << "]";                                    \
        blockScope->setPrimaryLog(__LINE__, CAPLOG_ss.str(), "");                             \
        CAP_LOG(__VA_ARGS__);                                                                 \
    } else if constexpr (channelCompileEnabledState) {                                        \
        blockScope->setPrimaryLog(__LINE__, "", "");                                          \
    }

#define CAP_LOG_INTERNAL_CHANNEL_EXPAND_NS(pointer, channel, ...) \
    CAP_LOG_INTERNAL(pointer, CAP::CHANNEL:: channel, __VA_ARGS__)

#define CAP_LOG_SCOPE(...) CAP_LOG_INTERNAL_CHANNEL_EXPAND_NS(this, __VA_ARGS__)
#define CAP_LOG_SCOPE_NO_THIS(...) CAP_LOG_INTERNAL_CHANNEL_EXPAND_NS(nullptr, __VA_ARGS__)

/// you may optionally provide an argument in the form of "(format, ...)"
// Deprecated
// legacy macros that use CAP::CHANNEL as namespaces
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
    if constexpr (CAP::Channel<CAP::as_sequence<channel>::type>::enableMode() & CAP::CAN_WRITE_TO_OUTPUT) {         \
        CAP::TLSScope tlsScope(__FILENAME__, __PRETTY_FUNCTION__);                                \
        if (tlsScope.anonymousBlockLog != nullptr) {                                              \
            std::stringstream CAPLOG_ss;                                                          \
            CAPLOG_ss << " [" << __LINE__      \
                      << "]::[" << __FILENAME__ \
                      << "]::[" << __PRETTY_FUNCTION__ << "]";                         \
            tlsScope.anonymousBlockLog->setPrimaryLog(__LINE__, CAPLOG_ss.str(), "");             \
        }                                                                                         \
        PRAGMA_IGNORE_SHADOW_BEGIN                                                                \
        CAP::BlockLogger* blockScope = tlsScope.blockLog;                                         \
        PRAGMA_IGNORE_SHADOW_END                                                                  \
        CAP_LOG_IMPL(__VA_ARGS__);                                                                \
    }

#define CAP_LOG_ERROR_ANONYMOUS(channel, ...)                                                     \
    if constexpr (CAP::Channel<CAP::as_sequence<channel>::type>::enableMode() & CAP::CAN_WRITE_TO_OUTPUT) {         \
        CAP::TLSScope tlsScope(__FILENAME__, __PRETTY_FUNCTION__);                                \
        if (tlsScope.anonymousBlockLog != nullptr) {                                              \
            std::stringstream CAPLOG_ss;                                                          \
            CAPLOG_ss << " [" << __LINE__      \
                      << "]::[" << __FILENAME__ \
                      << "]::[" << __PRETTY_FUNCTION__ << "]";                         \
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
    using varName##_type = std::conditional<CAP::Channel<CAP::as_sequence<channel>::type>::enableMode() != 0, \
                                            varTypeIfEnabled, varTypeIfDisabled>::type;     \
    std::any varName =                                                                      \
            CAP::Channel<CAP::as_sequence<channel>::type>::enableMode() != 0 ? varInitIfEnabled : varInitIfDisabled;

#define CAP_SCAN_BLOCK_NO_THIS(...) CAP_LOG_BLOCK_NO_THIS(__VA_ARGS__)
#define CAP_SCAN_BLOCK(...) CAP_LOG_BLOCK(__VA_ARGS__)
#define CAP_SCAN(...) CAP_LOG(__VA_ARGS__)

#define CAP_DUMP_TO_FILE(filename, pointerToBuffer, numberOfBytes)                  \
    if constexpr (channelCompileEnabledOutput) {                                    \
        blockScope->dumpToFile(__LINE__, filename, pointerToBuffer, numberOfBytes); \
    }

#else

#define CAP_LOGGER_ONLY [[maybe_unused]]

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

#define DEFINE_CAP_LOG_CHANNEL(...)
#define DEFINE_CAP_LOG_CHANNEL_CHILD(...)

#endif





