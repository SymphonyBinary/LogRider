#pragma once

// #define CHANNELS_PATH channeldefs.hpp
#ifndef CHANNELS_PATH
static_assert(false, "CHANNELS_PATH not defined");
#endif

#include <iomanip>

#include "constants.hpp"
#include "output.hpp"
#include "utilities.hpp"

namespace CAP {

enum class CHANNEL {
#define CAPTAINS_LOG_CHANNEL(name, ...) name,
#define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
#define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
#include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
#undef CAPTAINS_LOG_CHANNEL
#undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
#undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
    COUNT,
};

inline std::string_view channelToString(CHANNEL channel) {
    static std::array<std::string, (size_t)CHANNEL::COUNT> strings = {
#define CAPTAINS_LOG_CHANNEL(name, ...) #name,
#define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
#define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
#include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
#undef CAPTAINS_LOG_CHANNEL
#undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
#undef CAPTAINS_LOG_CHANNEL_END_CHILDREN
    };
    return strings[(size_t)channel];
}

enum ChannelEnabledFlags : uint32_t {
    CAN_WRITE_TO_STATE = 1 << 1,
    CAN_WRITE_TO_OUTPUT = 1 << 2,
    ALL_FLAGS = std::numeric_limits<uint32_t>::max(),
};

enum ChannelEnabledMode : uint32_t {
    FULLY_DISABLED = 0,
    FULLY_ENABLED = ALL_FLAGS,
    ENABLED_NO_OUTPUT = ALL_FLAGS ^ CAN_WRITE_TO_OUTPUT,
};

constexpr std::array<uint32_t, (size_t)CHANNEL::COUNT> getChannelFlagMap() {
    std::array<uint32_t, (size_t)CHANNEL::COUNT> flags{};
    std::array<uint32_t, (size_t)CHANNEL::COUNT> parentFlags = {ALL_FLAGS};
    int index = 0;
    int parentFlagsIndex = 0;
    uint32_t currentEnabledMode = ALL_FLAGS;

#define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabledMode)         \
    currentEnabledMode = enabledMode & parentFlags[parentFlagsIndex]; \
    flags[index++] = currentEnabledMode;

#define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
    parentFlags[++parentFlagsIndex] = currentEnabledMode;

#define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) --parentFlagsIndex;

#include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)

#undef CAPTAINS_LOG_CHANNEL
#undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
#undef CAPTAINS_LOG_CHANNEL_END_CHILDREN

    return flags;
}

inline void printChannel(std::stringstream& ss, unsigned int processId, unsigned int threadId,
                         unsigned int depth, unsigned int channelId, std::string_view channelName,
                         uint32_t enabledMode, int verbosityLevel) {
    ss << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
       << CAP_PROCESS_ID_DELIMITER << processId << " " << CAP_THREAD_ID_DELIMITER
       << threadId
       << " CHANNEL-ID=" << std::setw(3) << std::setfill('0') << channelId;

    if (enabledMode == FULLY_ENABLED) {
        ss << " : FULLY ENABLED        ";
    } else if (enabledMode == ENABLED_NO_OUTPUT) {
        ss << " : ENABLED BUT NO OUTPUT";
    } else if (enabledMode == FULLY_DISABLED) {
        ss << " : FULLY DISABLED       ";
    } else {
        ss << " : UNKNOWN MODE!        ";
    }

    ss << " : VERBOSITY=" << verbosityLevel << " : ";

    for (unsigned int i = 0; i < depth; ++i) {
        ss << ">  ";
    }

    ss << channelName << CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
}

inline void printLogLineCharacterLimit(std::stringstream& ss, unsigned int processId) {
    ss << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
       << CAP_PROCESS_ID_DELIMITER << processId << " " << CAP_MAX_CHAR_SIZE_DELIMITER
       << CAP::OutputModeToLogLineCharLimit[static_cast<int>(CAP::DefaultOutputMode)]
       << CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)];
}

}  // namespace CAP
