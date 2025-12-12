#pragma once

// #define CHANNELS_PATH channeldefs.hpp
#ifndef CHANNELS_PATH
static_assert(false, "CHANNELS_PATH not defined");
#endif

#include <iomanip>

#include "constants.hpp"
#include "output.hpp"
#include "utilities.hpp"

// convenience macros to split up building this:
//      "constexpr const std::string_view SV_ ## channelname {#channelname};"
// and 
//      "struct Channel<::CAP::as_sequence<SV_ ## channelname>::type> {"
// multiple macros aren't strictly needed but they add convenience elsewhere too.

#define CAP_LOG_SV_CHANNEL(channelname) SV_ ## channelname

#define DEFINE_CAP_LOG_CHANNEL(channelname, verboseLevel, enabledMode) \
namespace CAP { \
DEFINE_CAP_LOG_CHANNEL_IMPL(channelname, verboseLevel, enabledMode) \
}

#define DEFINE_CAP_LOG_CHANNEL_IMPL(channelname, verboseLevel, enabledMode) \
CAP_LOG_CHANNEL_DEFINE_STRING_VIEW_IMPL(channelname) \
DEFINE_CAP_LOG_CHANNEL_FROM_CONSTEXPR_STRINGVIEW_IMPL(channelname, verboseLevel, enabledMode)

#define CAP_LOG_CHANNEL_DEFINE_STRING_VIEW_IMPL(channelname) \
  constexpr const std::string_view CAP_LOG_SV_CHANNEL(channelname) {#channelname};

// TODO print once what the mode is in english (eg. enabled || enabled and printing)
#define DEFINE_CAP_LOG_CHANNEL_FROM_CONSTEXPR_STRINGVIEW_IMPL(channelname, verboseLevel, enabledMode) \
template <> \
struct Channel<::CAP::as_sequence<CAP_LOG_SV_CHANNEL(channelname)>::type> { \
    static size_t id() { \
      static size_t uniqueID = ChannelID::getNextChannelUniqueID(); \
      return uniqueID; \
    } \
    constexpr static ChannelEnabledMode enableMode() { \
      return enabledMode; \
    } \
    constexpr static int verbosityLevel() { \
      return verboseLevel; \
    } \
};

#define CAP_CHANNEL(channel) \
CAP::Channel<CAP::as_sequence<CAP::SV_ ## channel>::type>

#define CAP_CHANNEL_OUTPUT_MODE(channel) \
CAP_CHANNEL(channel)::enableMode()

namespace CAP {

struct ChannelID {
    static size_t getNextChannelUniqueID() {
        static std::atomic<size_t> currentChannelUniqueID = 0;
        return currentChannelUniqueID++;
    }
};


///// string stuff
/////

// This helper if just a template forward declaration which helps to also
// automatically generate an index sequence from the size of the input string.
// The fully specialized version of this class is specialized for receiving an
// index sequence, and can specify its template params (the incrementing indices) 
// in a way where they can be unpacked to use as actual indices into the string_view.
template<const std::string_view& sv,
         class Seq = std::make_index_sequence<sv.size()>>
struct as_sequence;

template<const std::string_view& sv, std::size_t ...II>
struct as_sequence<sv, std::index_sequence<II...>> {
    using type=std::integer_sequence<char, sv[II]...>;
};

// strings as template types which allow for "string" -> unique type conversion.
// from https://stackoverflow.com/questions/1826464/c-style-strings-as-template-arguments
template <char... chars>
using tstring = std::integer_sequence<char, chars...>;

// template <typename>
// struct Channel {
//   constexpr static bool isEnabled() {
//     return false;
//   }
// }

// constexpr std::string_view SVDefault{"DEFAULT"};
// constexpr std::string_view SVValidation{"VALIDATION"};

// template<>
// struct Channel<as_sequence<SVDefault>::type> {
//   constexpr static bool isEnabled() {
//     return true;
//   }
// };

// template<>
// struct Channel<as_sequence<SVValidation>::type> {
//   constexpr static bool isEnabled() {
//     return true;;
//   }
// };

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

template <typename>
struct Channel {
  static size_t id() {
    static size_t uniqueID = ChannelID::getNextChannelUniqueID();
    return uniqueID;
  }
  constexpr static ChannelEnabledMode enableMode() {
    return ChannelEnabledMode::FULLY_DISABLED;
  }
  constexpr static int verbosityLevel() {
    return 0;
  }
};

// declare the default channel.  We're already in the CAP namespace,
// se we use the inner "impl" versions.
DEFINE_CAP_LOG_CHANNEL_IMPL(DEFAULT, 0, ChannelEnabledMode::FULLY_ENABLED);

// #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabledMode) \
//     constexpr std::string_view SV_ ## name {#name}; \
// template <> \
// struct Channel<as_sequence<SV_ ## name>::type> { \
//     constexpr static ChannelEnabledMode enableMode() { \
//       return enabledMode; \
//     } \
//     constexpr static int verbosityLevel() { \
//       return verboseLevel; \
//     } \
// };

// #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabledMode) \
//     DEFINE_CAP_LOG_CHANNEL(name, verboseLevel, enabledMode)

// #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
// #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
// #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
// #undef CAPTAINS_LOG_CHANNEL
// #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
// #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN

/////
/////



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

// enum ChannelEnabledFlags : uint32_t {
//     CAN_WRITE_TO_STATE = 1 << 1,
//     CAN_WRITE_TO_OUTPUT = 1 << 2,
//     ALL_FLAGS = std::numeric_limits<uint32_t>::max(),
// };

// enum ChannelEnabledMode : uint32_t {
//     FULLY_DISABLED = 0,
//     FULLY_ENABLED = ALL_FLAGS,
//     ENABLED_NO_OUTPUT = ALL_FLAGS ^ CAN_WRITE_TO_OUTPUT,
// };

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

    // #define CAPTAINS_LOG_CHANNEL(name, ...) \
    // { \
    //     constexpr static std::string_view SVString{#name}; \
    //     bool enabled = Channel<as_sequence<SVString>::type>::isEnabled(); \
    //     printf("Channel %s is %s\n", SVString.data(), enabled ? "enabled" : "disabled"); \
    // }
    // #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...)
    // #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...)
    // #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
    // #undef CAPTAINS_LOG_CHANNEL
    // #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
    // #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN


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
