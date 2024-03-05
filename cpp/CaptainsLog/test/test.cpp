#include "../src/caplogger.hpp"
#include <vector>
#include <sstream>

namespace something{

class TestRender {
public:
  TestRender(std::string name) {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    (void)name;
    CAP_LOG_SET_STATE("Current Name", [&](std::optional<std::string>){return name.c_str();})
    CAP_LOG_SET_STATE("Current Name 1", [&](std::optional<std::string>){return name.c_str();})
    CAP_LOG_SET_STATE("Current Name 2", [&](std::optional<std::string>){return name.c_str();})
    CAP_LOG_SET_STATE("Current Name 3", [&](std::optional<std::string>){return name.c_str();})
  }

  ~TestRender() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
    CAP_LOG_RELEASE_ALL_STATE();
  }

  void testBlockOutput() {
    CAP_LOG_BLOCK(CAP::CHANNEL::RENDER);
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
    // CAP_LOG_SET_STATE(static_cast<std::ostringstream&>(std::stringstream{} << "asdf " << 1).str().c_str(),[&](std::optional<std::string>){return "asdfasdf";})
  }

  void doSomething(){
    CAP_LOG_BLOCK(CAP::CHANNEL::AUDIO_SUB_CHANNEL_Z);
    doIt();
  }
};



int main() {
  CAP_LOG_BLOCK_NO_THIS(CAP::CHANNEL::DEFAULT, "main");

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