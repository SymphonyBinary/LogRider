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
      if(prev) {
        return prev.value() + " APPENDED SOME STUFF";
      }})
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
    something::ExampleStateWithThisPtr exampleState;
    exampleState.makeEntity(57, "Square thing");
    exampleState.makeEntity(42, "Triangle thing");
    exampleState.makeEntity(12, "Airplane");
    exampleState.makeEntity(18, "Zig");
    exampleState.connectEntities(57, 12);
    exampleState.connectEntities(18, 12);
    exampleState.connectEntities(42, 57);

    addStateToExampleState(exampleState);
  }



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