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
    CAP_LOG_SET_STATE(std::to_string(id), [&](std::optional<std::string>){return name.c_str();})
  }

  void connectEntities(int idA, int idB) {
    CAP_LOG_BLOCK(CAP::CHANNEL::SET_THIS_EXAMPLE, "Connecting %d and %d", idA, idB);
    CAP_LOG_PRINT_STATE(std::to_string(idA));
    CAP_LOG_PRINT_STATE(std::to_string(idB));
  }
};
/// ---- 


/// ---- demonstrates how set state can target other addresses too
void addStateToExampleState(const ExampleStateWithThisPtr& example) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_ADDRESS_EXAMPLE);
  CAP_LOG_SET_STATE_ON_ADDRESS(&example, "SomeStateInsertedOutside", [&](std::optional<std::string>){return "outside state!";});
  CAP_LOG_SET_STATE_ON_ADDRESS(&example, "Variable Name Can Have Spaces", [&](std::optional<std::string>){return "more outside state!";});
}
/// ----


/// ---- demonstrates how set state can be used on objects with a string name.
void addStateToStoreName(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_SET_STATE_ON_STORE_NAME(storeName, name, [&](std::optional<std::string>){return "default value";})
}

void changeStateToStoreName(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_SET_STATE_ON_STORE_NAME(storeName, name, [&](std::optional<std::string>){return "changed value";})
}

void printStore(std::string storeName, std::string name) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_PRINT_STATE_ON_STORE_NAME(storeName, name);
}

void printAllStore(std::string storeName) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_PRINT_ALL_STATE_ON_STORE_NAME(storeName);
}

void releaseAllStateOnStore(std::string storeName) {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::SET_STORE_NAME_EXAMPLE);
  CAP_LOG_RELEASE_ALL_STATE_ON_STORE_NAME(storeName);
}
/// ----

class TestRender {
public:
  TestRender(std::string name) {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_SET_STATE("Current Name", [&](std::optional<std::string>){return name.c_str();})
  }

  ~TestRender() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_RELEASE_ALL_STATE();
  }

  void testBlockOutput() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_SET_STATE("Current Name", [&](std::optional<std::string> prev){
        return (prev ? prev.value() + " APPENDED SOME STUFF" : "no state yet");})
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
    CAP_LOG_RELEASE_STATE(static_cast<std::ostringstream&>(std::stringstream{} << "asdf " << 1).str().c_str());
  }

  void doIt() {
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_OTHER);
    CAP_LOG_SET_STATE(static_cast<std::ostringstream&>(std::stringstream{} << "asdf " << 1).str().c_str(), [&](std::optional<std::string>){return "asdfasdf";})
  }

  void doSomething(){
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_Z);
    doIt();
  }
};



int main() {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT, "main");

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT);
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
    CAP_LOG_PRINT_All_STATE_ON_ADDRESS(&exampleState);
  }

  {
    CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT);
    CAP_LOG("This demonstrates writing writing to named stores");

    something::addStateToStoreName("global state", "foo");
    something::addStateToStoreName("global state", "bar");
    something::addStateToStoreName("global state", "third");
    something::changeStateToStoreName("global state", "bar");
    something::printAllStore("global state");

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