#pragma once

#include "channels.hpp"
#include "output.hpp"
#include "utilities.hpp"

#include <vector>
#include <array>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <chrono>

namespace CAP {

enum class ValueChangeStatus {
  UNCHANGED,
  CREATED,
  UPDATED,
  DELETED,
};

template<size_t DATA_COUNT>
using NumChangedElementsN = ArrayN<ValueChangeStatus, DATA_COUNT>;

using DataStoreKey = std::variant<const void*, const char*, std::string>;
template<size_t DATA_COUNT>
using DataStoreKeysArrayN = ArrayN<DataStoreKey, DATA_COUNT>;

template<typename... Args>
DataStoreKeysArrayN<sizeof...(Args)> storeKeyList(const Args&... args) {
  return DataStoreKeysArrayN<sizeof...(Args)>{{args...}};
}

// DataStoreMemberVariableName are analagous to member variables of objects
using DataStoreMemberVariableName = std::string;
template<size_t DATA_COUNT>
using DataStoreMemberVariableNamesArrayN = ArrayN<DataStoreMemberVariableName, DATA_COUNT>;

template<typename... Args>
DataStoreMemberVariableNamesArrayN<sizeof...(Args)> variableNames(const Args&... args) {
  return DataStoreMemberVariableNamesArrayN<sizeof...(Args)>{{args...}};
}

using DataStoreState = std::optional<std::string>;
template<size_t DATA_COUNT>
using DataStoreStateArray = std::array<DataStoreState, DATA_COUNT>;
template<size_t DATA_COUNT>
using DataStoreValuesArrayUpdater = std::function<void(DataStoreStateArray<DATA_COUNT>&)>;

inline const std::string& to_string(ValueChangeStatus changes) {
  switch (changes) {
    case ValueChangeStatus::UNCHANGED: 
      static std::string unchanged = "UNCHANGED";
      return unchanged;
    case ValueChangeStatus::CREATED:
      static std::string created = "CREATED";
      return created;
    case ValueChangeStatus::UPDATED:
      static std::string updated = "UPDATED";
      return updated;
    case ValueChangeStatus::DELETED:
      static std::string deleted = "DELETED";
      return deleted;
  }
  static std::string defaultRet;
  return defaultRet;
}

inline std::string to_string(const DataStoreKey& key) {
  std::string retString;
  std::visit(overloaded{
      [&](const std::string& storeKey) {retString = storeKey;},
      [&](const char* storeKey) {retString = std::string(storeKey);},
      [&](const void* storeKey) {
        retString = std::to_string(reinterpret_cast<uintptr_t>(storeKey));
      },
    }, 
    key);
  return retString;
}

// TODO make test that shows it works with fork and .so

struct LoggerData {
  int logDepth = -1;
  int perThreadUniqueFunctionIdx = -1;
  unsigned int relativeThreadIdx = 0;
  size_t processTimestampInstanceKey = 0;
  bool isNew = true;
};

class DataStore {
public:
  template<size_t DATA_COUNT>
  DataStoreStateArray<DATA_COUNT> getStates(
      const DataStoreKeysArrayN<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames) {
    auto ret = DataStoreStateArray<DATA_COUNT>();
    for (size_t i = 0; i < DATA_COUNT; ++i){
      std::visit(overloaded{
        [&](const std::string& storeKey) {
          ret[i] = mDataStoreStrings[storeKey][stateNames[i]];
        },
        [&](const char* storeKey) {
          ret[i] = mDataStoreStrings[std::string(storeKey)][stateNames[i]];
        },
        [&](const void* storeKey) {
          ret[i] = mDataStorePointer[storeKey][stateNames[i]];
        },
      }, 
      storeKeys[i]);
    }
    return ret;
  }

  // intentionally returning by copy for threading reasons.  Be careful if this is too large.
  std::unordered_map<std::string, std::optional<std::string>> getAllStates(
      DataStoreKey storeKey) {
    std::unordered_map<std::string, std::optional<std::string>> retVal;
    auto* variablesOfStorage = getVariablesForStore(storeKey);
    if (variablesOfStorage) {
      retVal = *variablesOfStorage;
    }
    return retVal;
  }

  // keysN = DataStoreKeysArrayN<DATA_COUNT>
  template<class keysN, size_t DATA_COUNT = keysN::Size>
  NumChangedElementsN<DATA_COUNT> setStates(
      const keysN& storeKeys, 
      const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames,
      DataStoreStateArray<DATA_COUNT>&& newStates) {
    static_assert(std::is_same<keysN, DataStoreKeysArrayN<DATA_COUNT>>::value, 
                  "storeKeys must be of type DataStoreKeysArrayN<DATA_COUNT>");
    NumChangedElementsN<DATA_COUNT> retStatus;        
    for (size_t i = 0; i < DATA_COUNT; ++i) {
      DataStoreState* currentState = nullptr;
      std::visit(overloaded{
        [&](const std::string& storeKey) {
          currentState = &mDataStoreStrings[storeKey][stateNames[i]];
        },
        [&](const char* storeKey) {
          currentState = 
                &mDataStoreStrings[std::string(storeKey)][stateNames[i]];
        },
        [&](const void* storeKey) {
          currentState = &mDataStorePointer[storeKey][stateNames[i]];
        },
      }, 
      storeKeys[i]);

      if(*currentState == newStates[i]) {
        retStatus[i] = ValueChangeStatus::UNCHANGED;
      } else if (!newStates[i]) {
        retStatus[i] = ValueChangeStatus::DELETED;
      } else if (!(*currentState)) {
        retStatus[i] = ValueChangeStatus::CREATED;
      } else {
        retStatus[i] = ValueChangeStatus::UPDATED;
      }

      *currentState = std::move(newStates[i]);
    }
    return retStatus;
  }

  std::optional<std::string> releaseState(DataStoreKey storeKey, 
      const DataStoreMemberVariableName& stateName) {
    std::optional<std::string> deletedValueRet;
    auto* variablesOfStorage = getVariablesForStore(storeKey);
    if (variablesOfStorage) {
      if (auto stateFind = variablesOfStorage->find(stateName); 
          stateFind != variablesOfStorage->end()) {
        deletedValueRet = stateFind->second;
        variablesOfStorage->erase(stateFind);
      }
    }
    return deletedValueRet;
  }

  int releaseAllStates(DataStoreKey storeKey) {
    int stateDeletedCountRet = 0;
    std::visit(overloaded{
      [&](const std::string& key) {
        if (const auto& objectFind = mDataStoreStrings.find(key); 
            objectFind != mDataStoreStrings.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStoreStrings.erase(objectFind);
        }
      },
      [&](const char* key) {
        std::string keyAsString = key;
        if (const auto& objectFind = mDataStoreStrings.find(keyAsString); 
            objectFind != mDataStoreStrings.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStoreStrings.erase(objectFind);
        }
      },
      [&](const void* key) {
        if (const auto& objectFind = mDataStorePointer.find(key); 
            objectFind != mDataStorePointer.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStorePointer.erase(objectFind);
        }
      },
    }, 
    storeKey);

    return stateDeletedCountRet;
  }

private:
  // Can better optimize this by templating a function with specializations for the types  
  std::unordered_map<std::string, std::optional<std::string>>* getVariablesForStore(
      const DataStoreKey& storeKey) {
    std::unordered_map<std::string, std::optional<std::string>>* retPtr = nullptr;
    std::visit(overloaded{
      [&](const std::string& key) {
        if (const auto& objectFind = mDataStoreStrings.find(key); 
            objectFind != mDataStoreStrings.end()) {
          retPtr = &objectFind->second;
        }
      },
      [&](const char* key) {
        std::string keyAsString = key;
        if (const auto& objectFind = mDataStoreStrings.find(keyAsString); 
            objectFind != mDataStoreStrings.end()) {
          retPtr = &objectFind->second;
        }
      },
      [&](const void* key) {
        if (const auto& objectFind = mDataStorePointer.find(key); 
            objectFind != mDataStorePointer.end()) {
          retPtr = &objectFind->second;
        }
      },
    }, 
    storeKey);

    return retPtr;
  }

  // storage for pointers
  std::unordered_map<const void*, std::unordered_map<std::string, std::optional<std::string>>> 
      mDataStorePointer;
  // storage for strings & const chars (implicitly cast to string).
  std::unordered_map<std::string, std::unordered_map<std::string, std::optional<std::string>>> 
      mDataStoreStrings;
};

struct BlockLoggerDataStore {
  static BlockLoggerDataStore& getInstance();
  
  static int getNextThreadId() {
    static std::atomic<int> id{0};
    return id++;
  }

  static LoggerData& getThreadLocalLoggerData() {
    thread_local LoggerData loggerData;
    return loggerData;
  }

  // must call this immediately after a ::fork() call to update
  // mProcessTimestampInstanceKey.
  //
  // WARNING: it's critical that you close all other threads prior to
  // calling fork.
  //
  // Reminder, fork() will only retain the thread where the fork was called.
  // If you have threads still open before calling fork(), and those threads
  // are executing code which are decorated with CAPS_LOGS, it's undefined
  // behavior at that point and you may or may not end up with unexpected caplog
  // scope blocks created and destroyed for the parent or child process.
  void onChildFork();

  LoggerData newBlockLoggerInstance() {
        auto& data = getThreadLocalLoggerData();

        if (data.isNew) {
            data.isNew = false;
            data.processTimestampInstanceKey = mProcessTimestampInstanceKey;
            data.relativeThreadIdx = getNextThreadId();
            PRINT_TO_LOG("New Thread. New ThreadID: " + std::to_string(data.relativeThreadIdx) +
                         CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)]);
        } else if (data.processTimestampInstanceKey != mProcessTimestampInstanceKey) {
            // logger data already exists, but was associated with a different process Id.
            // this only happens immediately after forking.  We will treat these blocks as though
            // they're on new threads.  If there's an existing block scope still existing on this
            // thread post-fork, (eg. if fork() is called inside a block scope), that scope's
            // destructor will still properly update with the new process id, but still use the
            // thread id from before the fork.
            // to the processor, it will look as though is sees a block closing at a depth
            // larger than it expects for the new process, which it allows.

            data = LoggerData{};
            data.isNew = false;
            data.processTimestampInstanceKey = mProcessTimestampInstanceKey;
            data.relativeThreadIdx = getNextThreadId();
            PRINT_TO_LOG("New Process: [" + std::to_string(data.processTimestampInstanceKey) +
                         "] | Thread remap: [" + std::to_string(data.relativeThreadIdx) + "]" +
                         CAP::OutputModeToNewLineChar[static_cast<int>(CAP::DefaultOutputMode)]);
        }

        ++data.logDepth;
        ++data.perThreadUniqueFunctionIdx;

        return data;
    }

    void removeBlockLoggerInstance() {
        auto& data = getThreadLocalLoggerData();
        data.processTimestampInstanceKey = mProcessTimestampInstanceKey;
        --data.logDepth;
    }

    template <size_t DATA_COUNT>
    DataStoreStateArray<DATA_COUNT> getStates(
            const DataStoreKeysArrayN<DATA_COUNT>& storeKeys,
            const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames) {
        const std::lock_guard<std::mutex> guard(mMut);
        return mCustomLogStateStores.getStates(storeKeys, stateNames);
    }

    std::unordered_map<std::string, std::optional<std::string>> getAllStates(
            const DataStoreKey& storeKey) {
        const std::lock_guard<std::mutex> guard(mMut);
        return mCustomLogStateStores.getAllStates(storeKey);
    }

    template <size_t DATA_COUNT>
    NumChangedElementsN<DATA_COUNT> updateStates(
            const DataStoreKeysArrayN<DATA_COUNT>& storeKeys,
            const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames,
            DataStoreStateArray<DATA_COUNT>&& newStates) {
        const std::lock_guard<std::mutex> guard(mMut);
        return mCustomLogStateStores.setStates(storeKeys, stateNames, std::move(newStates));
    }

    std::optional<std::string> releaseState(DataStoreKey storeKey,
                                            const DataStoreMemberVariableName& stateName) {
        const std::lock_guard<std::mutex> guard(mMut);
        return mCustomLogStateStores.releaseState(storeKey, stateName);
    }

    int releaseAllStates(DataStoreKey storeKey) {
        const std::lock_guard<std::mutex> guard(mMut);
        return mCustomLogStateStores.releaseAllStates(storeKey);
    }

    BlockLoggerDataStore(const BlockLoggerDataStore&) = delete;
    void operator=(const BlockLoggerDataStore&) = delete;

  private:
    BlockLoggerDataStore();

    size_t generateProcessTimestampInstanceKey();

    // this value is only ever written to when the singleton is created (thread safe)
    // and immediately after a fork (thread safe, because you should only have 1 thread
    // before calling fork)
    size_t mProcessTimestampInstanceKey = 0;

    // mutex guards the custom log state store
    std::mutex mMut;
    DataStore mCustomLogStateStores;
};

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
