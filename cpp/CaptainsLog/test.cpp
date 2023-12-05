#define ENABLE_CAP_LOGGER

#include "caplogger.hpp"
#include <vector>

namespace something{

class TestOne {
public:
  TestOne(std::string name) {
    CAP_LOG_BLOCK();
    (void)name;
    CAP_SET("Current Name", "%s", name.c_str());
  }
  void testBlockOutput() {
    CAP_LOG_BLOCK();
  }
};

class TestTwo {
public:
  std::vector<TestOne> tests;

  TestTwo() {
    CAP_LOG_BLOCK("Testing format = %s", "hello");
    for(int i = 0; i < 5; ++i ){
      tests.emplace_back("In Two " + std::to_string(i));
      tests.back().testBlockOutput();
    }
  }
};
}

class NestTest1 {
public:
  void doIt() {
    CAP_LOG_BLOCK();
    CAP_SET("123","asdfasdf");
  }

  void doSomething(){
    CAP_LOG_BLOCK();
    doIt();
  }
};



int main() {
  CAP_LOG_BLOCK_NO_THIS("main");

  NestTest1 nestTest1;
  nestTest1.doSomething();

  {
    CAP_LOG_BLOCK_NO_THIS();
    something::TestOne t("first");
    t.testBlockOutput();
    CAP_LOG("this is a log");
  }

  CAP_ERROR("this is an error");
  something::TestTwo two;

  CAP_LOG("this is a log with formatting %s ", "FORMAT");

  something::TestOne t1("t1");
  something::TestOne t2("t2");
  something::TestOne t3("t3");

  CAP_LOG();

  t3.testBlockOutput();
  t2.testBlockOutput();
  t1.testBlockOutput();

  return 0;
}