#define ENABLE_CAP_LOGGER
#ifdef ENABLE_CAP_LOGGER

#include "capdata.hpp"
#include "channels.hpp"
#include "output.hpp"

#include <chrono>

namespace CAP {

size_t getPid() {
#if defined LINUX || defined __LINUX__ || defined ANDROID || defined __ANDROID__ || \
        defined APPLE || defined __APPLE__
    return ::getpid();
#endif
    return 0;
}

size_t getTimeSinceEpochMs() {
    return static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count());
}

size_t generatePidTimestampKey() {
    return getPid() ^ getTimeSinceEpochMs();
}

// Gnarly problem with dynamic libs: singletons can be instantiated multiple times
// in the same app through dynamic libs.  We need to treat each instance of the singleton
// as completely unique new processes, even if it's the same
// to do this, we can use a combination of address+timestamp+pid
/*static*/ BlockLoggerDataStore& BlockLoggerDataStore::getInstance() {
    static BlockLoggerDataStore instance;
    // PRINT_TO_LOG("Singleton instance: " +
    // std::to_string(reinterpret_cast<uintptr_t>((void*)&instance)) +
    // CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)]);
    return instance;
}

BlockLoggerDataStore::BlockLoggerDataStore() {
    mProcessTimestampInstanceKey = generateProcessTimestampInstanceKey();
    mCustomLogStateStores = {};

    // This should be the first thing caplog prints, at least on the first thread caplog
    // is run on.  Other threads may still interleave while this is printing, which is
    // fine.
    PRINT_TO_LOG(std::string("CAP_LOG : CAPTAIN'S LOG - VERSION 1.3 : Address: ") +
                 std::to_string(reinterpret_cast<uintptr_t>((void*)this)) +
                 CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)]);

    auto logData = newBlockLoggerInstance();

    // Print the max chars per line
    std::stringstream ss;
    printLogLineCharacterLimit(ss, logData.processTimestampInstanceKey);
    PRINT_TO_LOG(ss.str().c_str());

    // Print the enabled status of all the channels.
    int channelDepth = 0;
    int channelId = 0;
    ss.str("");

#define CAPTAINS_LOG_CHANNEL(name, verboseLevel, channelEnabledMode)                               \
    printChannel(ss, logData.processTimestampInstanceKey, logData.relativeThreadIdx, channelDepth, \
                 channelId, #name, CAP::getChannelFlagMap()[channelId], verboseLevel);             \
    ++channelId;                                                                                   \
    PRINT_TO_LOG(ss.str().c_str());                                                                \
    ss.str("");
#define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) ++channelDepth;
#define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) --channelDepth;

#include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
#undef CAPTAINS_LOG_CHANNEL
#undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
#undef CAPTAINS_LOG_CHANNEL_END_CHILDREN

    removeBlockLoggerInstance();
}

void BlockLoggerDataStore::onChildFork() {
    mProcessTimestampInstanceKey = generateProcessTimestampInstanceKey();
    PRINT_TO_LOG(std::string("Child Forked.  Generating new Process timestamp key") +
                 CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)]);
}

size_t BlockLoggerDataStore::generateProcessTimestampInstanceKey() {
    return generatePidTimestampKey() ^ (uintptr_t)(void*)this;
}

}  // namespace CAP

#endif