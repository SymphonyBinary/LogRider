#include "include/caplogger.hpp"
#include <vector>
#include <sstream>

// constexpr const std::string_view channelA{"CHANNEL_ALPHA"};

DEFINE_CAP_LOG_CHANNEL(CHANNEL_ALPHA, 2, CAP::ChannelEnabledMode::FULLY_ENABLED)

int main() {
  CAP_LOG_BLOCK_NO_THIS(DEFAULT, "main");
  // CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT, "main");

  printf("Channel CHANNEL_ALPHA is %s\n", CAP_CHANNEL_OUTPUT_MODE(CHANNEL_ALPHA) ? "enabled" : "disabled");

  printf("Channel DEFAULT is %s\n", CAP::Channel<CAP::as_sequence<CAP::SV_DEFAULT>::type>::enableMode() ? "enabled" : "disabled");

  return 0;
}

//CAP::Channel<CAP::as_sequence<CAP::SV_ ## channel>::type>::mode()

// constexpr static std::string_view SVChannel{#channel}; \
//   [[maybe_unused]] constexpr bool channelCompileNotDisabled = CAP::Channel<CAP::as_sequence<SVChannel>::type>::mode(); \