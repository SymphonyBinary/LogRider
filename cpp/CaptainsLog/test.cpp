#include "dereklogger.h"

class TestOne {
public:
  void testBlockOutput() {
    D_LOG_BLOCK();
  }
};

int main() {
  TestOne t;
  t.testBlockOutput();

  return 0;
}