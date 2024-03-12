#ifdef ENABLE_CAP_LOGGER

#include "capdata.hpp"

namespace CAP {

/*static*/ BlockLoggerDataStore& BlockLoggerDataStore::getInstance() {
  static BlockLoggerDataStore instance;
  //PRINT_TO_LOG("Singleton instance: %p", (void*)&instance);
  return instance;
}

} // namespace CAP

#endif
