#pragma once

#ifdef ENABLE_CAP_LOGGER

#include <string>

#ifdef ANDROID
#include <android/log.h>
#endif

namespace CAP {

inline void writeToLogcat([[maybe_unused]] const std::string& output) {
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_ERROR, "CAPLOG_TAG", "%s", output.c_str());
#endif
}

inline void writeToStandardOut(const std::string& output) {
    printf("%s", output.c_str());
}

inline void writeToPlatformOut(const std::string& output) {
#ifdef ANDROID
    writeToLogcat(output);
#else
    writeToStandardOut(output);
#endif
}

}  // namespace CAP

#endif  // ENABLE_CAP_LOGGER