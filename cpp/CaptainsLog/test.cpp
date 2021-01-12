#include "clogger.h"
#include <vector>

namespace something{

class TestOne {
public:
  TestOne(std::string name) {
    C_LOG_BLOCK();
    (void)name;
    C_SET("Current Name", "%s", name.c_str());
  }
  void testBlockOutput() {
    C_LOG_BLOCK();
  }
};

class TestTwo {
public:
  std::vector<TestOne> tests;

  TestTwo() {
    C_LOG_BLOCK("Testing format = %s", "hello");
    for(int i = 0; i < 5; ++i ){
      tests.emplace_back("In Two " + std::to_string(i));
      tests.back().testBlockOutput();
    }
  }
};
}

int main() {
  C_LOG_BLOCK_NO_THIS("main");

  {
    C_LOG_BLOCK_NO_THIS();
    something::TestOne t("first");
    t.testBlockOutput();
    C_LOG("this is a log");
  }

  C_ERROR("this is an error");
  something::TestTwo two;

  C_LOG("this is a log with formatting %s ", "FORMAT");

  something::TestOne t1("t1");
  something::TestOne t2("t2");
  something::TestOne t3("t3");

  C_LOG();

  t3.testBlockOutput();
  t2.testBlockOutput();
  t1.testBlockOutput();

  return 0;
}