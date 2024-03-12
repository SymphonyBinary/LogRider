#pragma once

#ifdef ENABLE_CAP_LOGGER

#include <chrono>
#include <vector>
#include <array>
#include <iomanip>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <limits>
#include <optional>
#include <variant>
#include <type_traits>

#include <cstdint>

namespace CAP {

namespace {
  template<class... Ts>
  struct overloaded : Ts... { using Ts::operator()...; };
  // explicit deduction guide (not needed as of C++20)
  template<class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;
} // namespace

template<class T, size_t N>
struct ArrayN {
  ArrayN(std::array<T, N> array)
      : v(std::move(array)){}

  ArrayN() = default;

  const T& operator[] (int index) const {
    return v[index];
  }

  T& operator[] (int index) {
    return v[index];
  }
  
  constexpr static size_t Size = N;
  std::array<T, N> v;
};

enum class ValueChangeStatus {
  UNCHANGED,
  CREATED,
  UPDATED,
  DELETED,
};

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

inline std::string to_string(const DataStoreKey& key) {
  std::string retString;
  std::visit(overloaded{
      [&](const std::string& storeKey) {retString = storeKey;},
      [&](const char* storeKey) {retString = std::string(storeKey);},
      [&](const void* storeKey) {retString = std::to_string(reinterpret_cast<uintptr_t>(storeKey));},
    }, key);
  return retString;
}

struct LoggerData {
  unsigned int logDepth = 0;
  unsigned int perThreadUniqueFunctionIdx = 0;
  unsigned int relativeThreadIdx = 0;
  unsigned int processTimestamp = 0;
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
        [&](const std::string& storeKey) {ret[i] = mDataStoreStrings[storeKey][stateNames[i]];},
        [&](const char* storeKey) {ret[i] = mDataStoreStrings[std::string(storeKey)][stateNames[i]];},
        [&](const void* storeKey) {ret[i] = mDataStorePointer[storeKey][stateNames[i]];},
      }, storeKeys[i]);
    }
    return ret;
  }

  // intentionally returning by copy for threading reasons.  Be careful if this is too large.
  std::unordered_map<std::string, std::optional<std::string>> getAllStates(DataStoreKey storeKey) {
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
    static_assert(std::is_same<keysN, DataStoreKeysArrayN<DATA_COUNT>>::value, "storeKeys must be of type DataStoreKeysArrayN<DATA_COUNT>");
    NumChangedElementsN<DATA_COUNT> retStatus;        
    for (size_t i = 0; i < DATA_COUNT; ++i) {
      DataStoreState* currentState = nullptr;
      std::visit(overloaded{
        [&](const std::string& storeKey) {currentState = &mDataStoreStrings[storeKey][stateNames[i]];},
        [&](const char* storeKey) {currentState = &mDataStoreStrings[std::string(storeKey)][stateNames[i]];},
        [&](const void* storeKey) {currentState = &mDataStorePointer[storeKey][stateNames[i]];},
      }, storeKeys[i]);

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

  std::optional<std::string> releaseState(DataStoreKey storeKey, const DataStoreMemberVariableName& stateName) {
    std::optional<std::string> deletedValueRet;
    auto* variablesOfStorage = getVariablesForStore(storeKey);
    if (variablesOfStorage) {
      if (auto stateFind = variablesOfStorage->find(stateName); stateFind != variablesOfStorage->end()) {
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
        if (const auto& objectFind = mDataStoreStrings.find(key); objectFind != mDataStoreStrings.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStoreStrings.erase(objectFind);
        }
      },
      [&](const char* key) {
        std::string keyAsString = key;
        if (const auto& objectFind = mDataStoreStrings.find(keyAsString); objectFind != mDataStoreStrings.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStoreStrings.erase(objectFind);
        }
      },
      [&](const void* key) {
        if (const auto& objectFind = mDataStorePointer.find(key); objectFind != mDataStorePointer.end()) {
          stateDeletedCountRet = objectFind->second.size();
          mDataStorePointer.erase(objectFind);
        }
      },
    }, storeKey);

    return stateDeletedCountRet;
  }

private:
  // Can better optimize this by templating a function with specializations for the types  
  std::unordered_map<std::string, std::optional<std::string>>* getVariablesForStore(const DataStoreKey& storeKey) {
    std::unordered_map<std::string, std::optional<std::string>>* retPtr = nullptr;
    std::visit(overloaded{
      [&](const std::string& key) {
        if (const auto& objectFind = mDataStoreStrings.find(key); objectFind != mDataStoreStrings.end()) {
          retPtr = &objectFind->second;
        }
      },
      [&](const char* key) {
        std::string keyAsString = key;
        if (const auto& objectFind = mDataStoreStrings.find(keyAsString); objectFind != mDataStoreStrings.end()) {
          retPtr = &objectFind->second;
        }
      },
      [&](const void* key) {
        if (const auto& objectFind = mDataStorePointer.find(key); objectFind != mDataStorePointer.end()) {
          retPtr = &objectFind->second;
        }
      },
    }, storeKey);

    return retPtr;
  }

  // storage for pointers
  std::unordered_map<const void*, std::unordered_map<std::string, std::optional<std::string>>> mDataStorePointer;
  // storage for strings & const chars (implicitly cast to string).
  std::unordered_map<std::string, std::unordered_map<std::string, std::optional<std::string>>> mDataStoreStrings;
};

struct BlockLoggerDataStore {
  static BlockLoggerDataStore& getInstance();
  
  LoggerData newBlockLoggerInstance() {
    const std::lock_guard<std::mutex> guard(mMut);

    auto threadId = std::this_thread::get_id();

    bool newThreadId = (mData.find(threadId) == mData.end());
    //PRINT_TO_LOG("newThreadId = %s", newThreadId ? "true" : "false");

    auto& counters = mData[threadId];
    if(!newThreadId) {
      ++counters.logDepth;
      ++counters.perThreadUniqueFunctionIdx;
    } else {
      counters.processTimestamp = mProcessTimestamp;
      counters.relativeThreadIdx = mUniqueThreadsSeen++;
    }

    return counters;
  }

  void removeBlockLoggerInstance(){
    const std::lock_guard<std::mutex> guard(mMut);

    auto threadId = std::this_thread::get_id();
    auto& data = mData.at(threadId); //will throw if it can't find, which we want
    --data.logDepth;
  }

  template<size_t DATA_COUNT>
  DataStoreStateArray<DATA_COUNT> getStates(
      const DataStoreKeysArrayN<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStores.getStates(storeKeys, stateNames);
  }

  std::unordered_map<std::string, std::optional<std::string>> getAllStates(const DataStoreKey& storeKey) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStores.getAllStates(storeKey);
  }

  template<size_t DATA_COUNT>
  NumChangedElementsN<DATA_COUNT> updateStates(
      const DataStoreKeysArrayN<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArrayN<DATA_COUNT>& stateNames,
      DataStoreStateArray<DATA_COUNT>&& newStates) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStores.setStates(storeKeys, stateNames, std::move(newStates));
  }

  std::optional<std::string> releaseState(DataStoreKey storeKey, const DataStoreMemberVariableName& stateName) {
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
  BlockLoggerDataStore() 
  : mProcessTimestamp(static_cast<unsigned int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count())) {
  }

  std::mutex mMut;
  std::unordered_map<std::thread::id, LoggerData> mData;
  unsigned int mUniqueThreadsSeen = 0;
  //special timestamp used as a key to identify this process from others.
  const unsigned int mProcessTimestamp = 0;

  DataStore mCustomLogStateStores;
};

} // namespace CAP

#endif
