#pragma once

#include <iomanip>

namespace CAP {

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

}

