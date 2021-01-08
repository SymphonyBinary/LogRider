#include "clogger.h"
#include <vector>

namespace something{

class TestOne {
public:
  TestOne(std::string name) {
    C_LOG_BLOCK();
    C_SET("Current Name", name);
  }
  void testBlockOutput() {
    C_LOG_BLOCK();
  }
};

class TestTwo {
public:
  std::vector<TestOne> tests;

  TestTwo() {
    C_LOG_BLOCK();
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
  }

  something::TestTwo two;

  return 0;
}