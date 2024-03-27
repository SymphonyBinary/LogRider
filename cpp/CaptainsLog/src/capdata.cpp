#ifdef ENABLE_CAP_LOGGER

#include "include/capdata.hpp"
#include "include/channels.hpp"

#include <chrono>

namespace CAP {

/*static*/ BlockLoggerDataStore& BlockLoggerDataStore::getInstance() {
  static BlockLoggerDataStore instance;
  //PRINT_TO_LOG("Singleton instance: %p", (void*)&instance);
  return instance;
}

BlockLoggerDataStore::BlockLoggerDataStore() 
  : mProcessTimestamp(static_cast<unsigned int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count())) {

  // This should be the first thing caplog prints.
  PRINT_TO_LOG("CAP_LOG : CAPTAIN'S LOG - VERSION 1.2");

  auto logData = newBlockLoggerInstance();

  // Print the max chars per line
  std::stringstream ss;
  printLogLineCharacterLimit(ss, logData.processTimestamp);
  PRINT_TO_LOG("%s", ss.str().c_str());

  // Print the enabled status of all the channels.
  int channelDepth = 0;
  int channelId = 0;
  ss.str("");

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, channelEnabledMode) \
  printChannel(ss, logData.processTimestamp, logData.relativeThreadIdx, channelDepth, channelId, #name, CAP::getChannelFlagMap()[channelId], verboseLevel); \
  ++channelId; \
  PRINT_TO_LOG("%s", ss.str().c_str()); \
  ss.str("");
  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  ++channelDepth;
  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  --channelDepth;

  #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)
  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN

  removeBlockLoggerInstance();
}

} // namespace CAP

#endif
