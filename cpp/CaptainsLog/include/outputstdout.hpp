#pragma once

#include <string>

#ifdef ANDROID
#include <android/log.h>
#elif defined(__APPLE__)
#include <os/log.h>
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
#elif defined(__APPLE__)
    os_log_info(OS_LOG_DEFAULT, "[CAPLOG] %{public}s", output.c_str());
#else
    writeToStandardOut(output);
#endif
}

}  // namespace CAP
