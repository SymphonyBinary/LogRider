#ifndef ENABLE_CAP_LOGGER
#define ENABLE_CAP_LOGGER
#endif

#include "../src/caplogger.hpp"
#include <vector>
#include <sstream>

namespace something{

/// ---- demonstrates how set state can be used on objects with a this pointer.  It will automatically 
/// use that address
class ExampleStateWithThisPtr {
public:
  ExampleStateWithThisPtr() = default;

  ~ExampleStateWithThisPtr() {
    CAP_LOG_BLOCK(CAP::CHANNEL::SET_THIS_EXAMPLE);
    CAP_LOG_RELEASE_ALL_STATE();
  }

  void makeEntity(int id, std::string name) {
    CAP_LOG_BLOCK(CAP::CHANNEL::SET_THIS_EXAMPLE);
    CAP_LOG_UPDATE_STATE(CAP::string(id), [&](CAP::DataStoreStateArray<1>& state){state[0] = name;});
  }

  void connectEntities(int idA, int idB) {
    CAP_LOG_BLOCK(CAP::CHANNEL::SET_THIS_EXAMPLE, "Connecting %d and %d", idA, idB);
    CAP_LOG_PRINT_STATE(CAP::string(idA));
    CAP_LOG_PRINT_STATE(CAP::string(idB));
  }
};
/// ---- 


/// ---- demonstrates how set state can target other addresses too
void addStateToExampleState(const ExampleStateWithThisPtr& example) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_ADDRESS_EXAMPLE);
  CAP_LOG_UPDATE_STATE_ON(
    CAP::storeKeyList(&example), 
    CAP::variableNames("SomeStateInsertedOutside"), 
    [&](CAP::DataStoreStateArray<1>& state){state[0] = "outside state!";});
  CAP_LOG_UPDATE_STATE_ON(
    CAP::storeKeyList(&example), 
    CAP::variableNames("Variable Name Can Have Spaces"), 
    [&](CAP::DataStoreStateArray<1>& state){state[0] = "more outside state!";});
}
/// ----


/// ---- demonstrates how set state can be used on objects with a string name.
void addStateToStoreName(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_UPDATE_STATE_ON(CAP::storeKeyList(storeName), CAP::variableNames(name), [&](CAP::DataStoreStateArray<1>& state){state[0] = "default value";})
}

void changeStateToStoreName(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_UPDATE_STATE_ON(CAP::storeKeyList(storeName), CAP::variableNames(name), [&](CAP::DataStoreStateArray<1>& state){state[0] = "changed value";})
}

void printStore(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_PRINT_STATE_ON(storeName, name);
}

void printAllStore(std::string storeName) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_PRINT_ALL_STATE_ON(storeName);
}

void releaseAllStateOnStore(std::string storeName) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_RELEASE_ALL_STATE_ON(storeName);
}
/// ----

class TestRender {
public:
  TestRender(std::string name) {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_UPDATE_STATE(CAP::variableNames("Current Name"), [&](CAP::DataStoreStateArray<1>& state){state[0] = name;})
  }

  ~TestRender() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_RELEASE_ALL_STATE();
  }

  void testBlockOutput() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_UPDATE_STATE(CAP::variableNames("Current Name"), [&](CAP::DataStoreStateArray<1>& state){
        state[0] = state[0].value_or("NO PREV STATE | ") + " APPENDED SOME STUFF";})
  }
};

class TestNetwork {
public:
  std::vector<TestRender> tests;

  TestNetwork() {
    CAP_LOG_BLOCK(CAP::CHANNEL::NETWORK, "Testing format = %s", "hello");
    for(int i = 0; i < 5; ++i ){
      tests.emplace_back("In Two " + std::to_string(i));
      tests.back().testBlockOutput();
    }
  }
};
}

class NestTest1 {
public:
  ~NestTest1(){
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_Z);
    // CAP_LOG_RELEASE_STATE(static_cast<const std::ostringstream&>(std::ostringstream{} << "asdf " << 1).str().c_str());
    CAP_LOG_RELEASE_ALL_STATE();
  }

  void doIt() {
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_OTHER);
    CAP_LOG_UPDATE_STATE(CAP::variableNames(CAP::string("asdf ", 1)), [&](CAP::DataStoreStateArray<1>& state){state[0] = "asdfasdf";})
  }

  void doSomething(){
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_Z);
    doIt();
  }
};

class TestNestedDisabledOutput {
public:
  TestNestedDisabledOutput(){
    CAP_LOG_BLOCK(CAP::CHANNEL::ENABLED_CHANNEL);
    disabledChannel();
  }

private:
  void disabledChannel() {
    CAP_LOG_BLOCK(CAP::CHANNEL::DISABLED_CHANNEL);
    noOutputChannel();
  }

  void noOutputChannel() {
    CAP_LOG_BLOCK(CAP::CHANNEL::NO_OUTPUT_CHANNEL);
    enabledChannel();
  }

  void enabledChannel() {
    CAP_LOG_BLOCK(CAP::CHANNEL::ENABLED_CHANNEL);
  }
};

int main() {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT, "main");

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_THIS_EXAMPLE);
    CAP_LOG("This demonstrates writing to the 'this' pointer, and writing to explicit addresses");
    something::ExampleStateWithThisPtr exampleState;
    exampleState.makeEntity(57, "Square thing");
    exampleState.makeEntity(42, "Triangle thing");
    exampleState.makeEntity(12, "Airplane");
    exampleState.makeEntity(18, "Zig");
    exampleState.connectEntities(57, 12);
    exampleState.connectEntities(18, 12);
    exampleState.connectEntities(42, 57);

    something::addStateToExampleState(exampleState);
    CAP_LOG("printing all state");
    CAP_LOG_PRINT_ALL_STATE_ON(&exampleState);
  }

  CAP_LOG("CAP::CHANNEL::UPDATE_STORE_NAME_EXAMPLE is disabled for output but still works for setting state!");

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
    CAP_LOG("This demonstrates writing writing to named stores");

    something::addStateToStoreName("global state", "foo");
    something::addStateToStoreName("global state", "bar");
    something::addStateToStoreName("global state", "third");
    something::changeStateToStoreName("global state", "bar");
    something::printAllStore("global state");
  }

  CAP_LOG("printing all the state set in CAP::CHANNEL::UPDATE_STORE_NAME_EXAMPLE");

  CAP_LOG_PRINT_ALL_STATE_ON("global state");

  {
    CAP_LOG("Deleting state twice to show that second call has nothing left.")
    something::releaseAllStateOnStore("global state");
    something::releaseAllStateOnStore("global state");
  }

  CAP_LOG("Executing lambda if LAMBDA_EXAMPLE is enabled");
  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::LAMBDA_EXAMPLE);
    CAP_LOG_EXECUTE_LAMBDA([&](){
      std::cout << "print out GARBAAAGE!!" << std::endl;
    })
  }


  // the following is basically chaos.  TODO: organize this.

  NestTest1 nestTest1;
  nestTest1.doSomething();

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::MISC);
    something::TestRender t("first");
    t.testBlockOutput();
    CAP_LOG("this is a log");
  }

  TestNestedDisabledOutput t;

  CAP_LOG_ERROR("this is an error");
  something::TestNetwork two;

  CAP_LOG("this is a log with formatting %s ", "FORMAT");

  something::TestRender t1("t1");
  something::TestRender t2("t2");

  std::string giantString = "super";
  for(size_t i = 0; i < 100; ++i) {
    giantString += "long long long";
  }
  giantString += "string";

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::RENDER_SUB_CHANNEL_A_VERBOSE);
    CAP_LOG("%s", giantString.c_str());
  }


  something::TestRender t3("t3");

  CAP_LOG("last line before a bunch of testBlockOutput");

  t3.testBlockOutput();
  t2.testBlockOutput();
  t1.testBlockOutput();

  return 0;
}