#ifdef ENABLE_CAP_LOGGER

#include "caplogger.hpp"
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>


namespace CAP {


static const char* colourArray[] = {
  COLOUR BOLD CAP_RED,
  COLOUR BOLD CAP_GREEN,
  COLOUR BOLD CAP_YELLOW,
  COLOUR BOLD CAP_BLUE,
  COLOUR BOLD CAP_MAGENTA,
  COLOUR BOLD CAP_CYAN,
  COLOUR BOLD CAP_WHITE,
};
static const int colourArraySize = sizeof(colourArray)/sizeof(colourArray[0]);

//Before, I was using thread_local to store the per-thread state so that I wouldn't have to deal with mutexes or anything
//somehow on Android, thread_local didn't really do the expected things.  Also the process is

namespace {
  void printChannel(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelId, std::string_view channelName, uint32_t enabledMode, int verbosityLevel) {
    ss << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
      << processId << " " << THREAD_ID_DELIMITER << colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD CAP_GREEN 
      << " CHANNEL-ID=" << std::setw(3) << std::setfill('0') << channelId;

    if (enabledMode == FULLY_ENABLED) {
      ss << " : FULLY ENABLED        ";
    } else if (enabledMode == ENABLED_NO_OUTPUT) {
      ss << " : ENABLED BUT NO OUTPUT";
    } else if (enabledMode == FULLY_DISABLED) {
      ss << " : FULLY DISABLED       ";
    } else {
      ss << " : UNKNOWN MODE!        ";
    }

    ss << " : VERBOSITY=" << verbosityLevel << " : ";

    for(unsigned int i = 0 ; i < depth; ++i ) {
      ss << ">  ";
    }

    ss << channelName;
  }

  void printTab(std::stringstream& ss, unsigned int processId, unsigned int threadId, unsigned int depth, unsigned int channelId) {
    ss << COLOUR BOLD CAP_YELLOW << MAIN_PREFIX_DELIMITER << INSERT_THREAD_ID << " : " << PROCESS_ID_DELIMITER 
      << processId << " " << THREAD_ID_DELIMITER << colourArray[threadId % colourArraySize] << threadId << COLOUR BOLD CAP_GREEN 
      << " " << CHANNEL_ID_DELIMITER << std::setw(3) << std::setfill('0') << channelId << " ";

    for(unsigned int i = 0 ; i < depth; ++i ){
      ss << TAB_DELIMITER;
    }
  }


  /////////
  /// Expand an array into a parameter pack
  /// From: https://stackoverflow.com/questions/60434033/how-do-i-expand-a-compile-time-stdarray-into-a-parameter-pack
  template <auto arr, template <typename X, X...> typename Consumer,
            typename IS = decltype(std::make_index_sequence<arr.size()>())> struct Generator;

  //... specialized by this, which gets the index sequence from the previous line
  template <auto arr, template <typename X, X...> typename Consumer, std::size_t... I>
  struct Generator<arr, Consumer, std::index_sequence<I...>> {
    using type = Consumer<typename decltype(arr)::value_type, arr[I]...>;
  };

  /// Helper typename
  template <auto arr, template <typename T, T...> typename Consumer>
  using Generator_t = typename Generator<arr, Consumer>::type;

  /* Example
  
  /// Structure which wants to consume the array via a parameter pack.
  template <typename StructuralType, StructuralType... s> struct ConsumerStruct {
    constexpr auto operator()() const { return std::array{s...}; }
  };

  // Usage
  int main() {
    constexpr auto tup = std::array<int, 3>{{1, 5, 42}};
    constexpr Generator_t<tup, ConsumerStruct> tt;
    static_assert(tt() == tup);
    return 0;
  }
  */

  /////////

  template<unsigned int I>
  constexpr unsigned int templateConstant() {
      return I;
  }


  // template<typename... Args>
  // ArrayN<DataStoreMutableState, sizeof...(Args)> mutableStates(const Args&... args) {
  //   return ArrayN<DataStoreMutableState, sizeof...(Args)>({args...});
  // }

  template<class... Ts>
  struct overloaded : Ts... { using Ts::operator()...; };
  // explicit deduction guide (not needed as of C++20)
  template<class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  std::string to_string(const DataStoreKey& key) {
    std::string retString;
    std::visit(overloaded{
        [&](const std::string& storeKey) {retString = storeKey;},
        [&](const char* storeKey) {retString = std::string(storeKey);},
        [&](const void* storeKey) {retString = std::to_string(reinterpret_cast<uintptr_t>(storeKey));},
      }, key);
    return retString;
  }
} // namespace

struct LoggerData {
  unsigned int logDepth = 0;
  unsigned int perThreadUniqueFunctionIdx = 0;
  unsigned int relativeThreadIdx = 0;
  unsigned int processTimestamp = 0;
};

class DataStore {
public: 
  // template<unsigned int DATA_COUNT>
  // using DataStoreMutableStateArrayN = ArrayN<DataStoreMutableState, DATA_COUNT>;


// should be able to use regular arrays, because I can get the size with templateConstant
// then I need to take these three arrays, explode them into parameter pack (g)

// WAIT
// this function works by value, not reference!
// it grabs these values and returns them.  so returning optional values is fine.
// the higher level (blocklogger) is responsible for taking these values, and then using the same params,
// call WRITE!!!

/// therefore, we need
/// getStates (returns by value)
/// write states (takes by value)
/// the blockLogger thing will first
/// 1 - call get states with the params to build the input array
/// 2 - call the user lambda with the input array passed by output reference.  The user can then
/// write directly to it. (again, all std::optionals)
/// 3 - call the states with the same addresing params by with write this time
/// 3b - interpret setting an optional to a nullopt as equivalent to deleting that member? shouldn't ignore explicit 
/// attempt to set nullopt.
/// 4 - (optionally) log the values.
/// 
/// if we instead used pointers instead of by value so that we can skip the write part, that will make this much faster.
/// However, it will require a mutex to be held the entire time.  not good.  Better to just do the mutex, address into the arrays
/// get teh value, stop the mutex, run the lambda, then start the mutex again to write.


//... all this work just to use refs to unique_ptrs instead of pointrs to unique_ptrs (because pointer
// version can be build incrementally but refs can't.)
// actually, this work is pointless since you can't make a std::array of references anyways!
// actually X2, I can use a reference wrapper
// https://stackoverflow.com/questions/62253972/is-it-safe-to-reference-a-value-in-unordered-map
// unordered_maps should not affect the address of memory (so that references keep working.)

  template<size_t DATA_COUNT>
  DataStoreStateArray<DATA_COUNT> getStates(
      const DataStoreKeysArray<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArray<DATA_COUNT>& stateNames) {
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

  template<size_t DATA_COUNT>
  void setStates(
      const DataStoreKeysArray<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArray<DATA_COUNT>& stateNames,
      DataStoreStateArray<DATA_COUNT>&& newStates) {
    for (size_t i = 0; i < DATA_COUNT; ++i) {
      std::visit(overloaded{
        [&](const std::string& storeKey) {mDataStoreStrings[storeKey][stateNames[i]] = std::move(newStates[i]);},
        [&](const char* storeKey) {mDataStoreStrings[std::string(storeKey)][stateNames[i]] = std::move(newStates[i]);},
        [&](const void* storeKey) {mDataStorePointer[storeKey][stateNames[i]] = std::move(newStates[i]);},
      }, storeKeys[i]);
    }
  }

  // intentionally returning by copy for threading reasons.  Be careful if this is too large.
  std::unordered_map<std::string, std::optional<std::string>> getAllState(DataStoreKey storeKey) {
    std::unordered_map<std::string, std::optional<std::string>> retVal;
    auto* variablesOfStorage = getVariablesForStore(storeKey);
    return *variablesOfStorage;
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

  int releaseAllState(DataStoreKey storeKey) {
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


template<class KeyType>
class StateStore {
public:
  const std::string& setState(KeyType objectId, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    return (mStateStore[objectId][stateName] = stateUpdater(mStateStore[objectId][stateName])).value();
  }

  std::optional<std::string> getState(KeyType objectId, const std::string& stateName) const {
    if (const auto& objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      const auto& objectStateMap = objectFind->second;
      if (const auto& stateFind = objectStateMap.find(stateName); stateFind != objectStateMap.end()) {
        return stateFind->second;
      }
    }
    return std::nullopt;
  }

  // intentionally returning by copy for threading reasons.  Be careful if this is too large.
  std::unordered_map<std::string, std::optional<std::string>> getAllState(KeyType objectId) const {
    if (const auto& objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      return objectFind->second;
    }
    return {};
  }

  std::optional<std::string> releaseState(KeyType objectId, const std::string& stateName) {
    std::optional<std::string> deletedValueRet;
    if (auto objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      auto& objectStateMap = objectFind->second;
      if (auto stateFind = objectStateMap.find(stateName); stateFind != objectStateMap.end()) {
        deletedValueRet = stateFind->second;
        objectStateMap.erase(stateFind);
      }
    }
    return deletedValueRet;
  }

  int releaseAllState(KeyType objectId) {
    int stateDeletedCountRet = 0;
    if (auto objectFind = mStateStore.find(objectId); objectFind != mStateStore.end()) {
      auto& objectStateMap = objectFind->second;
      stateDeletedCountRet = objectStateMap.size();
      mStateStore.erase(objectFind);
    }
    return stateDeletedCountRet;
  }

private:
  std::unordered_map<KeyType, std::unordered_map<std::string, std::optional<std::string>>> mStateStore;
};

struct BlockLoggerDataStore {
  static BlockLoggerDataStore& getInstance() {
    static BlockLoggerDataStore instance;
    //PRINT_TO_LOG("Singleton instance: %p", (void*)&instance);
    return instance;
  }
  
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
      const DataStoreKeysArray<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArray<DATA_COUNT>& stateNames) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStores.getStates(storeKeys, stateNames);
  }

  template<size_t DATA_COUNT>
  void setStates(
      const DataStoreKeysArray<DATA_COUNT>& storeKeys, 
      const DataStoreMemberVariableNamesArray<DATA_COUNT>& stateNames,
      DataStoreStateArray<DATA_COUNT>&& newStates) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStores.setStates(storeKeys, stateNames, std::move(newStates));
  }

   // remove later
  const std::string& setState(const void* objectId, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.setState(objectId, stateName, stateUpdater);
  }

  std::optional<std::string> getState(const void* objectId, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.getState(objectId, stateName);
  }

  std::unordered_map<std::string, std::optional<std::string>> getAllState(const void* objectId) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.getAllState(objectId);
  }

  std::optional<std::string> releaseState(const void* objectId, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.releaseState(objectId, stateName);
  }

  int releaseAllState(const void* objectId) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStatePointers.releaseAllState(objectId);
  }
 
  const std::string& setStateStoreName(const std::string& storeName, const std::string& stateName, 
      std::function<std::string(std::optional<std::string>)>& stateUpdater) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.setState(storeName, stateName, stateUpdater);
  }

  std::optional<std::string> getStateStoreName(const std::string& storeName, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.getState(storeName, stateName);
  }

  std::unordered_map<std::string, std::optional<std::string>> getAllStateStoreName(const std::string& storeName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.getAllState(storeName);
  }

  std::optional<std::string> releaseStateStoreName(const std::string& storeName, const std::string& stateName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.releaseState(storeName, stateName);
  }

  int releaseAllStateStoreName(const std::string& storeName) {
    const std::lock_guard<std::mutex> guard(mMut);
    return mCustomLogStateStoreNames.releaseAllState(storeName);
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

  // for set/get/release commands.  Used for addresses.  Care must be taken not to send literal strings to this (which are just char*)
  StateStore<const void*> mCustomLogStatePointers;

  // for set/get/release commands.  Used for strings.
  StateStore<std::string> mCustomLogStateStoreNames;
};

/*static*/ BlockChannelTree& BlockChannelTree::getInstance() {
  static BlockChannelTree instance;
  return instance;
}

uint32_t BlockChannelTree::getEnabledMode(CHANNEL channel) {
  return mEnabledModeChannelsById[(size_t)channel];
}

BlockChannelTree::BlockChannelTree() {
  PRINT_TO_LOG("%s", "CAPTAIN'S LOG - VERSION 1.1"); \

  std::vector<uint32_t> enabledStack = {ALL_FLAGS};
  int index = 0;
  uint32_t currentEnabledMode = ALL_FLAGS;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, enabledMode) \
  currentEnabledMode = enabledMode & enabledStack.back(); \
  mEnabledModeChannelsById[index++] = currentEnabledMode;

  #define CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN(...) \
  enabledStack.emplace_back(currentEnabledMode);

  #define CAPTAINS_LOG_CHANNEL_END_CHILDREN(...) \
  enabledStack.pop_back();

  #include CAPTAINS_LOG_STRINGIFY(CHANNELS_PATH)

  #undef CAPTAINS_LOG_CHANNEL
  #undef CAPTAINS_LOG_CHANNEL_BEGIN_CHILDREN
  #undef CAPTAINS_LOG_CHANNEL_END_CHILDREN


  // Now print it.  This should be the first thing caplog prints.
  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();
  int channelDepth = 0;
  int channelId = 0;
  std::stringstream ss;

  #define CAPTAINS_LOG_CHANNEL(name, verboseLevel, channelEnabledMode) \
  printChannel(ss, logData.processTimestamp, logData.relativeThreadIdx, channelDepth, channelId++, #name, channelEnabledMode, verboseLevel); \
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
  
  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();
}


BlockLogger::BlockLogger(const void* thisPointer, CAP::CHANNEL channel)
  : mEnabledMode(BlockChannelTree::getInstance().getEnabledMode(channel)) 
  , mChannel(channel) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto logData = loggerDataStore.newBlockLoggerInstance();

  mDepth = logData.logDepth;
  mId = logData.perThreadUniqueFunctionIdx;
  mThreadId = logData.relativeThreadIdx;
  mProcessId = logData.processTimestamp;
  mThisPointer = thisPointer;

  updateState(1,
    storeKeyList("Flintstones", this, std::string("Jetsons")),
    variableNames("Fred", "Foo", "Wilma"), 
    [](DataStoreStateArray<3>& statesInOut){
      statesInOut[0] = "Barney";
      statesInOut[1] = std::nullopt;
      statesInOut[2] = "I'm Home";
    }
  );
}

void BlockLogger::setPrimaryLog(int line, std::string_view logInfoBuffer, std::string_view customMessageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  mlogInfoBuffer = std::move(logInfoBuffer.substr(0, LOG_INFO_BUFFER_LIMIT));
  if (mlogInfoBuffer.length() != logInfoBuffer.length()) {
    mlogInfoBuffer.append("...]");
  }
  mlogInfoBuffer.append(" ");

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);

  ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_BEGIN_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
  << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());

  if(customMessageBuffer.size() == 1) {
    return;
  }

  log(line, std::move(customMessageBuffer));
}

void BlockLogger::log(int line, std::string_view messageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  size_t index = 0;
  while(index < messageBuffer.size()) {
    // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] LOG: " 
    << messageBuffer.substr(index, LOG_LINE_CHARACTER_LIMIT) << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
    index += LOG_LINE_CHARACTER_LIMIT;
  }
}

void BlockLogger::error(int line, std::string_view messageBuffer) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  size_t index = 0;
  while(index < messageBuffer.size()) {
    // The macro which calls this hardcodes a " " to get around some macro limitations regarding zero/1/multi argument __VA_ARGS__
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "ERROR: " 
    << messageBuffer.substr(index, LOG_LINE_CHARACTER_LIMIT) << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
    index += LOG_LINE_CHARACTER_LIMIT;
  }
}

//-------- State Addresses

template<size_t DATA_COUNT, class UpdaterFunc>
void BlockLogger::updateState(
    int line, 
    const DataStoreKeysArray<DATA_COUNT>& keys, 
    const DataStoreMemberVariableNamesArray<DATA_COUNT>& varNames,
    const UpdaterFunc& stateUpdater) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto states =  loggerDataStore.getStates(keys, varNames);
  stateUpdater(states);

  if (mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    for (size_t i = 0; i < DATA_COUNT; ++i) {
      std::stringstream ss;
      printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
      ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
      << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "UPDATE STATE: "
      << "StoreKey='" << to_string(keys[i]) << "' : StateName='" << varNames[i] << "' : Value='" << (states[i] ? states[i].value() : "N/A") << "'" << COLOUR RESET;
      PRINT_TO_LOG("%s", ss.str().c_str());
    }
  }

  loggerDataStore.setStates(keys, varNames, std::move(states));
}

void BlockLogger::setState(int line, const void* address, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setState(address, stateName, stateUpdater);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
    << "Address=" << address << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::printCurrentState(int line, const void* address, const std::string& stateName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> state = loggerDataStore.getState(address, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
  << "Address=" << address << " : StateName='" << stateName << "' : Value='" << (state ? state.value() : "NO STATE") << "'"
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printAllCurrentState(int line, const void* address) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto allState = loggerDataStore.getAllState(address);

  for(const auto& row : allState) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
    << "Address=" << address << " : StateName='" << row.first << "' : Value='" << (row.second ? row.second.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseState(int line, const void* address, const std::string& stateName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseState(address, stateName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
    << "Address=" << address << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseAllState(int line, const void* address) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllState(address);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
    << "Address=" << address << " : Deleted State Count=" << deletedStateCount
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

//-------- State Store Name 

void BlockLogger::setStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName, 
    std::function<std::string(std::optional<std::string>)>& stateUpdater) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  const std::string& newState = loggerDataStore.setStateStoreName(stateStoreName, stateName, stateUpdater);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "SET STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << newState << "'" << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::printCurrentStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> state = loggerDataStore.getStateStoreName(stateStoreName, stateName);

  std::stringstream ss;
  printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
  ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
  << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
  << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << (state ? state.value() : "NO STATE") << "'"
  << COLOUR RESET;
  PRINT_TO_LOG("%s", ss.str().c_str());
}

void BlockLogger::printAllCurrentStateOnStoreName(int line, const std::string& stateStoreName) {
  if(!(mEnabledMode & CAN_WRITE_TO_OUTPUT)) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  auto allState = loggerDataStore.getAllStateStoreName(stateStoreName);

  for(const auto& row : allState) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "PRINT STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << row.first << "' : Value='" << (row.second ? row.second.value() : "NO STATE") << "'"
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseStateOnStoreName(int line, const std::string& stateStoreName, const std::string& stateName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  std::optional<std::string> deletedState = loggerDataStore.releaseStateStoreName(stateStoreName, stateName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE STATE: " 
    << "StoreName=" << stateStoreName << " : StateName='" << stateName << "' : Value='" << (deletedState ? deletedState.value() : "NO STATE") << "'" 
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

void BlockLogger::releaseAllStateOnStoreName(int line, const std::string& stateStoreName) {
  if(!mEnabledMode) {
    return;
  }

  auto& loggerDataStore = BlockLoggerDataStore::getInstance();
  int deletedStateCount = loggerDataStore.releaseAllStateStoreName(stateStoreName);

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << ADD_LOG_DELIMITER << ADD_LOG_SECOND_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << " "
    << COLOUR RESET << "[" << COLOUR BOLD CAP_GREEN << line << COLOUR RESET << "] " << COLOUR BOLD CAP_RED << "RELEASE ALL STATE: " 
    << "StoreName=" << stateStoreName << " : Deleted State Count=" << deletedStateCount
    << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

//------------------

BlockLogger::~BlockLogger() {
  if(!mEnabledMode) {
    return;
  }

  BlockLoggerDataStore::getInstance().removeBlockLoggerInstance();

  if(mEnabledMode & CAN_WRITE_TO_OUTPUT) {
    std::stringstream ss;
    printTab(ss, mProcessId, mThreadId, mDepth, (size_t)mChannel);
    ss << COLOUR BOLD CAP_GREEN << PRIMARY_LOG_END_DELIMITER << COLOUR BOLD CAP_YELLOW << " " << mId << mlogInfoBuffer
    << colourArray[reinterpret_cast<std::uintptr_t>(mThisPointer) % colourArraySize] << mThisPointer << COLOUR RESET;
    PRINT_TO_LOG("%s", ss.str().c_str());
  }
}

uint32_t BlockLogger::getEnabledMode() const {
  return mEnabledMode;
}

} // namespace CAP

#endif
