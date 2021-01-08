#include "captainslog.h"

class TestOne {
public:
  void testBlockOutput() {
    C_LOG_BLOCK();
  }
};

int main() {
  TestOne t;
  t.testBlockOutput();

  return 0;
}