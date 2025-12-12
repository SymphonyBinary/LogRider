#include "include/caplogger.hpp"
#include <vector>
#include <sstream>

DEFINE_CAP_LOG_CHANNEL(CHANNEL_ONE, 2, CAP::ChannelEnabledMode::FULLY_ENABLED)
  DEFINE_CAP_LOG_CHANNEL_CHILD(CHANNEL_TWO, 5, CAP::ChannelEnabledMode::FULLY_DISABLED, CHANNEL_ONE)
    DEFINE_CAP_LOG_CHANNEL_CHILD(CHANNEL_TWO_B, 8, CAP::ChannelEnabledMode::FULLY_ENABLED, CHANNEL_TWO) 
  DEFINE_CAP_LOG_CHANNEL_CHILD(CHANNEL_THREE, 8, CAP::ChannelEnabledMode::FULLY_ENABLED, CHANNEL_ONE)
    DEFINE_CAP_LOG_CHANNEL_CHILD(CHANNEL_THREE_B, 4, CAP::ChannelEnabledMode::ENABLED_NO_OUTPUT, CHANNEL_THREE)
      DEFINE_CAP_LOG_CHANNEL_CHILD(CHANNEL_THREE_C, 4, CAP::ChannelEnabledMode::FULLY_ENABLED, CHANNEL_THREE_B)
      

///////

int main() {

  // auto asdf = std::make_tuple(1,1,1, std::make_tuple(1,1,std::make_tuple(1, 1), 1), 1, std::make_tuple(1, 1, 1));
  // std::string flat_str = flatten_nested_tuple(asdf);

  // std::cout << "Flattened tuple: " << flat_str << std::endl;

  // std::apply([](auto&&... args) {
  //   // Use a fold expression to apply the printing logic to each argument
  //   ((std::cout << args << " "), ...);
  // }, asdf);
  
  CAP_LOG_BLOCK_NO_THIS(DEFAULT, "main");
  
  // CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT, "main");

  printf("Channel CHANNEL_ONE is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_ONE));
  printf("Channel CHANNEL_TWO is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_TWO));
  printf("Channel CHANNEL_TWO_B is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_TWO_B));
  printf("Channel CHANNEL_THREE is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_THREE));
  printf("Channel CHANNEL_THREE_B is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_THREE_B));
  printf("Channel CHANNEL_THREE_C is %zu\n", (size_t) CAP_CHANNEL_OUTPUT_MODE(CHANNEL_THREE_C));

  return 0;
}

//CAP::Channel<CAP::as_sequence<CAP::SV_ ## channel>::type>::mode()

// constexpr static std::string_view SVChannel{#channel}; \
//   [[maybe_unused]] constexpr bool channelCompileNotDisabled = CAP::Channel<CAP::as_sequence<SVChannel>::type>::mode(); \