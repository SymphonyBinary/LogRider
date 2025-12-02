#pragma once

#ifdef ENABLE_CAP_LOGGER

#include "outputstdout.hpp"

namespace CAP {

// Android needs it to be in /data/local/ to be able to read it; eg "/data/local/captains_log.clog"
#ifdef CAPLOGGER_LOG_TO_FILE
constexpr const char* outputFileName = CAPTAINS_LOG_STRINGIFY(CAPLOGGER_LOG_TO_FILE);
#else
constexpr const char* outputFileName = "captains_log.clog";
#endif

class FileLogger {
  public:
    static void writeToOutputFile(const std::string& output) {
        static FileLogger logger;
        if (logger.pFile != nullptr) {
            fprintf(logger.pFile, "%s", output.c_str());
            fflush(logger.pFile);
        }
    }

  private:
    FileLogger() {
        writeToPlatformOut(std::string("Opening CAPLOG log file: ") + outputFileName + "\n");
        pFile = fopen(outputFileName, "a");

        if (pFile == nullptr) {
            writeToPlatformOut(std::string("CAPLOG: Failed to open CAPLOG log file: ") +
                               outputFileName + "\n");
        } else {
            writeToPlatformOut(std::string("CAPLOG: Opened log file: ") + outputFileName + "\n");
        }
    }

    ~FileLogger() { fclose(pFile); }

  private:
    FILE* pFile;
};

}  // namespace CAP

#endif  // ENABLE_CAP_LOGGER