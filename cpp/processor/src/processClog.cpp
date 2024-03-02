#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>

/*
------------------------------------------------------------------------------
CHANNEL MESSAGE (always the first caplog message to get displayed per process)
------------------------------------------------------------------------------
1         2            3   4                5             6               7     8     
CAP_LOG : P=4165984483 T=0 CHANNEL-ID=002 : ENABLED=YES : VERBOSITY : 0 : >  >  RENDER_SUB_CHANNEL_A

1 - main delimiter
2 - process timestamp (used to uniquely identify this log to the process).  Remaps to a smaller number in vsix.
3 - relative thread id
4 - channel ID (3 digits)
5 - If the channel is enabled or not
6 - verbosity level
7 - tabs to show channel hierarchical relationship
8 - channel name

------------------------------------------------------------------------------
LOG LINES:
------------------------------------------------------------------------------

1         2            3   4     5  6 7     8           9                                               10
CAP_LOG : P=4293102038 T=0 C=005 :F 3 [25]::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
CAP_LOG : P=4293102038 T=0 C=005 :-> 3 [25] LOG: Testing format = hello 
CAP_LOG : P=4293102038 T=0 C=005 :L 3 [25]::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730

1 - main delimiter
2 - process timestamp (used to uniquely identify this log to the process).  Remaps to a smaller number in vsix.
3 - relative thread id
4 - channel ID (3 digits)
5 - PRIMARY_LOG_BEGIN_DELIMITER (start of block)
    F = start of block
    -> = log within block
    L = end of block
    : = function depth of logged functions (appends as prefix.  Eg. :F, :L, ::F, etc)
6 - per thread unique function idx (monotonic counter)
7 - line in the source file
8 - filename
9 - function name
10 - *this* pointer
*/

namespace {
/**
 * This will match all the logs:
 * CAP_LOG_BLOCK, CAP_LOG_BLOCK_NO_THIS, CAP_LOG, CAP_LOG_ERROR, CAP_SET
 * the sub-expressions:
 * 0 - the full string
 * 1 - ProcessId
 * 2 - ThreadId
 * 3 - ChannelId
 * 4 - Indentation
 * 5 - FunctionId
 * 6 - Line number in source code
 * 7 - Info string; the remainder of the string.  Changes depending on log type.
 **/
std::regex logLineRegex(
  ".*CAP_LOG : P=(.+?) T=(.+?) C=(.+?) (.+?) (.+?) (\\[.+?\\])(.*)",
  std::regex_constants::ECMAScript);

/**
 * This will match the opening and closing block tags
 * eg. ::[test.cpp]::[something::TestNetwork::TestNetwork()] 0x7ffecc005730
 * 0 - the full string
 * 1 - Filename
 * 2 - Function name (may be truncated if too long)
 * 3 - The object id.  In c++ this is the "this" pointer (or 0 if none)
 **/
std::regex infoStringBlockMatch(
  "::\\[(.*)\\]::\\[(.*)\\] ([0-9a-z]+)",
  std::regex_constants::ECMAScript
);

class WorldState {
public:    
  struct LoggedObject {
    std::string thisAddress;
    std::unordered_map<std::string, std::unordered_map<int, std::string>> pushedVariables;
  };

  struct StackNode {
    int line;
    int depth;
    std::string fileName;
    std::string functionName;
    std::unique_ptr<StackNode> caller;
    std::optional<LoggedObject> loggedObject;
  };

private:
  std::unordered_map<std::string, LoggedObject> mLoggedObjects;
  std::vector<StackNode> mLineData;
};


struct OutputState {
  std::string outputText;
  int logLineNumber = 0;


};

bool processLogLine(const std::string& inputLine, OutputState& outputState) {

}

size_t getFileSize(char* fileName) {
  std::ifstream file(fileName, std::ios::binary | std::ios::ate);
  return file.tellg();
}

} // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage: processClog [clogfile.clog]";
    return 0;
  }
  char* inputFileName = argv[1];

  size_t inputFileSize = getFileSize(inputFileName);

  std::ifstream fileStream(inputFileName);

  OutputState output;
  output.outputText.reserve(inputFileSize * 2);

  std::string inputLine;
  while (std::getline(fileStream, inputLine)) {
    // if (processLogLine)

    std::cmatch pieces_match;
    bool matched = std::regex_match (inputLine.c_str(), pieces_match, logLineRegex);
    if (matched) {
      // structured bindings don't work for regex match :(
      // cannot decompose inaccessible member ‘std::__cxx11::match_results<__gnu_cxx::__normal_iterator<...
      auto&& inputFullMatch = pieces_match[0];
      auto&& inputProcessId = pieces_match[1];   // 2
      auto&& inputThreadId = pieces_match[2];    // 3
      auto&& inputChannelId = pieces_match[3];   // 4
      auto&& inputIndentation = pieces_match[4]; // 5
      auto&& inputFunctionId = pieces_match[5];  // 6
      auto&& inputSourceLine = pieces_match[6];  // 7
      auto&& inputInfoString = pieces_match[7];  // 8


    }


    // for (std::size_t i = 0; i < pieces_match.size(); ++i) {
    //   std::ssub_match sub_match = pieces_match[i];
    //   std::string piece = sub_match.str();
    //   std::cout << "  submatch " << i << ": " << piece << '\n';
    // }
  }


  //std::cout << outputText << std::endl;

  return 0;
}

