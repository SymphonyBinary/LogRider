#pragma once

#include "basictypes.hpp"
#include "constants.hpp"
#include "output.hpp"
#include "utilities.hpp"
#include "datastore.hpp"
#include "configdefines.hpp"


#define CAP_VA_ARGS(...) , ##__VA_ARGS__

// convenience macros to split up building this:
//      "constexpr const std::string_view SV_ ## channelname {#channelname};"
// and 
//      "struct Channel<::CAP::as_sequence<SV_ ## channelname>::type> {"
// multiple macros aren't strictly needed but they add convenience elsewhere too.

#define CAP_LOG_CHANNEL_STRING(channelname) CAP::CHANNEL::channelname

// Public macros
// (... params are a list of parent channels that are AND'd together then AND'd with this channel)
// Example, if parent is disabled but this channel is enabled, final result will also be disabled.
#define DEFINE_CAP_LOG_CHANNEL(channelname, verboseLevel, enabledMode, ...) \
DEFINE_CAP_LOG_CHANNEL_CHILD_IMPL(channelname, verboseLevel, CAP::ChannelEnabledMode:: enabledMode, CHANNEL_ROOT_ALL_LOGS CAP_VA_ARGS(__VA_ARGS__))

////////////////
// Implementation Details
#define CHANNEL_OUTPUT_MODE_AND(...) channelOutputModeAnd<__VA_ARGS__>()

#define DEFINE_CAP_LOG_CHANNEL_IMPL(channelname, verboseLevel, enabledMode, overrideMode) \
namespace CAP::CHANNEL { \
constexpr const std::string_view channelname {#channelname}; \
DEFINE_CAP_LOG_CHANNEL_CHILD_FROM_CONSTEXPR_STRINGVIEW_IMPL(channelname, verboseLevel, enabledMode, overrideMode) \
}

#define DEFINE_CAP_LOG_CHANNEL_CHILD_IMPL(channelname, verboseLevel, enabledMode, ...) \
namespace CAP::CHANNEL { \
constexpr const std::string_view channelname {#channelname}; \
DEFINE_CAP_LOG_CHANNEL_CHILD_FROM_CONSTEXPR_STRINGVIEW_IMPL(channelname, verboseLevel, enabledMode, CHANNEL_OUTPUT_MODE_AND(__VA_ARGS__)) \
}

// TODO print once what the mode is in english (eg. enabled || enabled and printing)
#define DEFINE_CAP_LOG_CHANNEL_CHILD_FROM_CONSTEXPR_STRINGVIEW_IMPL(channelname, verboseLevel, enabledMode, inheritedOutputMode) \
template <> \
struct Channel<CAP::CHANNEL::as_sequence<channelname>::type> { \
    static size_t id() { \
      static size_t uniqueID = ChannelID::getNextChannelUniqueID(); \
      return uniqueID; \
    } \
    constexpr static uint32_t enableMode() { \
      return ForceEnableAllChannels ? ChannelEnabledMode::FULLY_ENABLED : inheritedOutputMode & enabledMode; \
    } \
    constexpr static int verbosityLevel() { \
      return verboseLevel; \
    } \
};

#define CAP_CHANNEL(channel) \
CAP::CHANNEL::Channel<CAP::CHANNEL::as_sequence<channel>::type>

#define CAP_CHANNEL_OUTPUT_MODE(channel) \
CAP_CHANNEL(channel)::enableMode()

namespace CAP::CHANNEL {

#ifdef CAP_LOGGER_FORCE_ALL_CHANNELS_ENABLED_IMPL
constexpr const bool ForceEnableAllChannels = true;
#else
constexpr const bool ForceEnableAllChannels = false;
#endif

struct ChannelID {
    static size_t getNextChannelUniqueID() {
        static std::atomic<size_t> currentChannelUniqueID = 0;
        return currentChannelUniqueID++;
    }
};

// struct ChannelPrinter {
//   ChannelPrinter() {
//     size_t processTimestampInstanceKey = BlockLoggerDataStore::getCurrentProcessTimestampInstanceKey();
//     std::stringstream ss;
//     ss << CAP_MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : "
//     PRINT_TO_LOG("CAPLOG: Initialized ChannelPrinter\n");
//   }
// }

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

template <typename>
struct Channel {
  static size_t id() {
    static size_t uniqueID = ChannelID::getNextChannelUniqueID();
    return uniqueID;
  }
  constexpr static uint32_t enableMode() {
    return ChannelEnabledMode::FULLY_DISABLED;
  }
  constexpr static int verbosityLevel() {
    return 0;
  }
};

// template<typename... Args>
template<const std::string_view& ...sv>
constexpr uint32_t channelOutputModeAnd() {
  return (CAP::CHANNEL::Channel<typename CAP::CHANNEL::as_sequence<sv>::type>::enableMode() & ...);
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

}  // namespace CAP::CHANNEL

// declare the default channels.  We're already in the CAP namespace,
// so we use the inner "impl" versions.  Root is default enabled.
DEFINE_CAP_LOG_CHANNEL_IMPL(CHANNEL_ROOT_ALL_LOGS,  0, ChannelEnabledMode::FULLY_ENABLED, CAP_LOGGER_ROOT_CHANNEL_MODE)
DEFINE_CAP_LOG_CHANNEL_CHILD_IMPL(DEFAULT, 0, ChannelEnabledMode::FULLY_ENABLED, CHANNEL_ROOT_ALL_LOGS)
DEFINE_CAP_LOG_CHANNEL_CHILD_IMPL(LEGACY, 0, ChannelEnabledMode::FULLY_ENABLED, CHANNEL_ROOT_ALL_LOGS, DEFAULT)

// Error logs are considered to be a different "top level"
DEFINE_CAP_LOG_CHANNEL_IMPL(CHANNEL_ROOT_ALL_ERRORS,  0, ChannelEnabledMode::FULLY_ENABLED, CAP_LOGGER_ROOT_CHANNEL_MODE)


// DEFINE_CAP_LOG_CHANNEL(ASDF, 0, FULLY_ENABLED)