#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <string_view>
#include <filesystem>

#include "process.hpp"

#include "behaviorTree.hpp"

#include <fcntl.h>

#define PORT 8427
#define TEMP_DIRECTORY_PATH std::filesystem::temp_directory_path()

constexpr const char* kSocketRawOutputFile = "validatorSocketRawInput";
constexpr const char* kParsedRawOutputFile = "validatorParsedRawInput.txt";
constexpr const char* kProcessedOutputFile = "validatorProcessed.clog";
constexpr const char* kValidatorReportFile = "validatorReport.txt";

// all validatorSocketRawInput files will be written in this directory.
// the parsed file and processed file should be the only things you want to look at.
constexpr const char* kRawSocketInputDir = "rawsocketinput";

// all the binary files that get dumped are written in this directory.
constexpr const char* kBinaryFileDumpDir = "binaryfiles";

namespace {

// Data driven; this can be replaced with json or deserialized in any other way.
//   This example works with artemis and validates some audio functionality.
BehaviorTree makeTreeExample() {
  BehaviorTree tree = {
    .root = BehaviorNodeRunAll (
      make_vector_unique<BehaviorNodeBase> (
        // This node records/memos all the encountered ctors and their associated addresses that they created.
        // It similarly removes the address:objects when encountering their dtors.
        BehaviorNodeMemoObjectPointer(),
        // triggers on this log:
        // "TestLabel | testKey: [testValue]"
        BehaviorNodeConditionalExecute(
          // This node's check condition only gets triggered when the label matches
          std::make_unique<ConditionScalarEquality<std::string>>( // trigger
            std::make_unique<FromScanLine>(FromScanLine::Label), // lhs
            std::make_unique<LiteralString>("TestLabel")),  // rhs
          std::make_unique<BehaviorNodeSequence>(
            make_vector_unique<BehaviorNodeBase>(
                BehaviorNodeCheckMessage(
                    std::make_unique<ConditionScalarEquality<std::string>>( // check
                        // this query gets the classname(string) associated with the id.
                        std::make_unique<QueryStateGetObjectClassFromId>( // lhs
                            std::make_unique<FromScanLine>(FromScanLine::Value, 0), // objectId
                            std::make_unique<FromStackNode>(FromStackNode::ProcessId)), // processId
                        std::make_unique<LiteralString>("testValue")))
                )
            )
          )))};
  return tree;
}

std::string getTimestampString() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::tm now_tm = *std::localtime(&now_c); // Use localtime for local time

  std::stringstream ss;
  ss << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S"); // Example format: YYYY-MM-DD_HH-MM-SS
  return ss.str();
}

struct StreamParser {
  constexpr static uint32_t headerDelimiter[2] = {0x12345678, 0x87654321};
  constexpr static size_t headerDelimiterSize = sizeof(uint32_t) * 2;

  enum class State {
    HeaderDelim,
    HeaderPayloadType,
    HeaderPaylodNumBytes,
    TextPayload,
    BinaryFilenamePayload,
    BinaryDataPayload,
  };

  State currentState = State::HeaderDelim;

  // lineLeftovers should always start with the full header (including the delimiter)
  std::string accumulatedLine = "";

  void parseLine(const char* buffer, size_t numBytes, std::vector<std::string>& stringsOut, std::vector<std::pair<std::string, std::vector<unsigned char>>>& bytesOut){
    // printf("parsing \n");
    std::string_view delim((const char*)headerDelimiter, (const char*)headerDelimiter + headerDelimiterSize);

    std::vector<size_t> delimiterIndices{};
    std::string_view line(buffer, buffer + numBytes);

    size_t lenthBeforeAppend = accumulatedLine.size();
    accumulatedLine += line;
    
    // delimiter may span across the end of the accumulated line and the new line, so we need to 
    // start our search "header size" before the accumulation point, (or 0, if the accumulated line is too small)
    // However, we also assume that the start of the accumulated line is a header.
    
    size_t from = (lenthBeforeAppend < headerDelimiterSize) ? 0 : (lenthBeforeAppend - headerDelimiterSize);
    from = std::max(from, headerDelimiterSize); // skip first delimiter, since we know accumulator starts with it.

    from = accumulatedLine.find(delim, from);
    while(from != std::string::npos) {
      // printf("delim: %zu \n", from);
      delimiterIndices.push_back(from);
      from = accumulatedLine.find(delim, from + 1);
    }

    // examine each line between delimiters.  lineleftovers should always start with the delimiter.
    size_t rangeStart = 0;
    for(size_t index : delimiterIndices) {
      size_t rangeEnd = index;
      
      size_t bodyTypeStart = rangeStart + headerDelimiterSize;
      size_t bodyLengthStart = bodyTypeStart + sizeof(uint32_t);
      size_t bodyStart = bodyLengthStart + sizeof(uint32_t);
      // printf("rangeStart: %zu | bodyTypeStart: %zu | bodyLengthStart: %zu | bodyStart: %zu | rangeEnd: %zu\n", rangeStart, bodyTypeStart, bodyLengthStart, bodyStart, rangeEnd);
      rangeStart = rangeEnd;

      if (bodyStart > rangeEnd) {
        assert(bodyStart < rangeEnd);
      }

      uint32_t payloadType = 0;
      memcpy(&payloadType, accumulatedLine.data() + bodyTypeStart, sizeof(uint32_t));

      uint32_t payloadLength = 0;
      memcpy(&payloadLength, accumulatedLine.data() + bodyLengthStart, sizeof(uint32_t));
      
      if (bodyStart == rangeEnd) {
        printf("empty body, likely an error.  bodyType = %zu | bodyPayloadLength = %zu \n", (size_t)payloadType, (size_t)payloadLength);
        assert(payloadLength == 0);
      }

      if (rangeEnd < bodyStart) {
        printf("MALFORMED \n");
        continue;
      }
      size_t length = rangeEnd - bodyStart;

      // printf("printing lines \n");
      // for(int i = bodyStart; i < rangeEnd; i++) {
      //   printf("%c", accumulatedLine[i]);
      // }

      // assume string for now
      if (payloadType == 0) {
        stringsOut.push_back(accumulatedLine.substr(bodyStart, length));
      } else if (payloadType == 1) {
        std::string_view substr = std::string_view(accumulatedLine).substr(bodyStart, length);
        if(size_t delim = substr.find("||"); delim != std::string::npos) {
          std::string_view filename = substr.substr(0, delim);
          std::string_view bytesStr = substr.substr(delim + 2);
          std::pair<std::string, std::vector<unsigned char>> filenameAndBytes;
          filenameAndBytes.first = filename;
          filenameAndBytes.second.resize(bytesStr.size());
          memcpy(filenameAndBytes.second.data(), bytesStr.data(), bytesStr.size());
          bytesOut.push_back(std::move(filenameAndBytes));
        }
      }
    }

    if (rangeStart != 0) {
      accumulatedLine = accumulatedLine.substr(rangeStart);
    }
  }
};

void runAsSocketServer(Processor&& processor) {
  std::ofstream parsedOutput;
  parsedOutput.open(kParsedRawOutputFile);

  std::filesystem::create_directory(kRawSocketInputDir);
  std::filesystem::create_directory(kBinaryFileDumpDir);

  int server_fd;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  // Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  std::cout << "Listening on port: " << PORT << std::endl;

  // Attach socket to the port
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // Bind
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  std::cout << "Listening to client " << std::endl;

  // Listen
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  std::vector<std::unique_ptr<std::thread>> threads;

  std::mutex clientLinesMut;
  std::mutex clientBytesMut;

  

  std::vector<std::string> clientLines{};
  std::vector<std::pair<std::string, std::vector<unsigned char>>> clientBytes{};

  // Thread that processes text and also executes the behavior tree
  threads.push_back(std::make_unique<std::thread>([&]() {
    std::vector<std::string> swapBuffer{};
    while (true) {
      {
        std::lock_guard<std::mutex> lock(clientLinesMut);
        if (clientLines.size() > 0) {
          swapBuffer = std::move(clientLines);
          clientLines = {};
        }
      }

      if (swapBuffer.size() > 0) {
        // std::cout << "swapBuffer lines to process: " << swapBuffer.size() << std::endl;
        for (auto& line : swapBuffer) {
          parsedOutput.write(line.c_str(), line.size());
          parsedOutput.flush();
          processor.readLine(line);
          processor.printOutputIfAvailable();
        }
        // std::cout << "swapBuffer Finished processing lines" << std::endl;
        swapBuffer = {};
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } 
  }));

  // Thread that processes binary data and writes it to the requested file.
  threads.push_back(std::make_unique<std::thread>([&]() {
    std::vector<std::pair<std::string, std::vector<unsigned char>>> swapBuffer{};
    while (true) {
      {
        std::lock_guard<std::mutex> lock(clientBytesMut);
        if (clientBytes.size() > 0) {
          swapBuffer = std::move(clientBytes);
          clientBytes = {};
        }
      }

      if (swapBuffer.size() > 0) {
        // std::cout << "swapBuffer lines to process: " << swapBuffer.size() << std::endl;
        for (const auto& filenameAndBytes : swapBuffer) {
          std::string binaryFilename = std::string(kBinaryFileDumpDir) + "/" + filenameAndBytes.first;
          std::ofstream binaryFileStream;
          binaryFileStream.open(binaryFilename, std::ios_base::app);
          binaryFileStream.write(reinterpret_cast<const char*>(filenameAndBytes.second.data()), filenameAndBytes.second.size());
          binaryFileStream.flush();
        }
        // std::cout << "swapBuffer Finished processing lines" << std::endl;
        swapBuffer = {};
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } 
  }));

  // Accept a connection
  while (true) {
    std::cout << "Checking for a new connection"  << std::endl;
    int new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

    if (new_socket < 0) {
      perror("accept");
      printf("error creating new socket\n");
      continue;
    }

    threads.push_back(std::make_unique<std::thread>([&, socketID = new_socket]() {
      std::string rawSocketFile = std::string(kRawSocketInputDir) + "/" + kSocketRawOutputFile + std::to_string(socketID) + ".txt";
      std::ofstream rawoutput;
      rawoutput.open(rawSocketFile);

      std::cout << "New connection, accepted connection on socket: " << socketID << std::endl;
      // extra bytes to add a newline to a read string for the purposes to dumping the line
      // to raw socket input file.
      char buffer[1024] = {0};
      int readChars = 0;

      StreamParser parser;

      while ((readChars = read(socketID, buffer, 1024)) != 0) { // 0 is eof; closed connection
        rawoutput.write(buffer, readChars);        
        std::vector<std::string> stringsOut{};
        std::vector<std::pair<std::string, std::vector<unsigned char>>> bytesOut{};

        parser.parseLine(buffer, readChars, stringsOut, bytesOut);
        
        if (!stringsOut.empty()) {
          std::lock_guard<std::mutex> lock(clientLinesMut);
          for(auto& line : stringsOut) {
            clientLines.push_back(std::move(line));
          }
        }

        if (!bytesOut.empty()) {
          std::lock_guard<std::mutex> lock(clientBytesMut);
          for(auto& filenameAndBytes : bytesOut) {
            clientBytes.push_back(std::move(filenameAndBytes));
          }
        }
      }

      std::cout << std::endl << "EOF.  Closing connection with id: " << socketID << std::endl;

      rawoutput.flush();
      rawoutput.close();
      close(socketID);
    }));
  }
  
  close(server_fd);
}

void runAsFileProcessor(Processor&& processor, const std::vector<std::string>& files) {
  std::ofstream parsedOutput;
  parsedOutput.open(kParsedRawOutputFile);

  for (const auto& filename : files) {
    std::cout << "Processing filename: " << filename << std::endl;

    std::ifstream fileStream(filename);
    if (!fileStream.is_open()) {
      std::cerr << "Error opening file: " << std::endl;
      if (fileStream.bad()) {
        std::cerr << "Fatal error: badbit is set." << std::endl;
      }
 
      if (fileStream.fail()) {
       std::cerr << "Error details: " << strerror(errno) << std::endl;
      }

      continue;
    }

    std::string inputLine;
    while (std::getline(fileStream, inputLine)) {
      parsedOutput << inputLine << std::endl;
      parsedOutput.flush();
      processor.readLine(inputLine);
    }
  }

  processor.printOutputIfAvailable();
}

} // namespace

int main(int argc, const char* argv[]) {
  std::string tempDirPath = TEMP_DIRECTORY_PATH;

  std::string sessionName = "trace_" + getTimestampString();

  std::string sessionPath = tempDirPath + "/" + sessionName;
  std::cout << std::endl << std::endl << "Temp directory is: " << std::endl; 
  std::cout << "*******************" << std::endl;
  std::cout << sessionPath << std::endl;
  std::cout << "*******************" << std::endl << std::endl;

  std::filesystem::create_directory(sessionPath);
  std::filesystem::current_path(sessionPath);

  // for(int i = 0; i < argc; i++) {
  //   std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
  // }

  std::ofstream validationReportOStream;
  validationReportOStream.open(kValidatorReportFile);

  std::unique_ptr<BehaviorTree> tree = nullptr;
#define USE_TREE_EXAMPLE 1
#if USE_TREE_EXAMPLE
  // tree should be changed to be loaded in through json or some other means.
  tree = std::make_unique<BehaviorTree>(makeTreeExample());
  tree->state.validationOStream = &validationReportOStream;
#else
  validationReportOStream << "Nothing to report.  No validation tree loaded." << std::endl;
#endif

  std::ofstream processedOStream;
  processedOStream.open(kProcessedOutputFile);

  Processor processor(std::move(processedOStream), tree.get());

  if (argc > 1) {
    std::cout << "Mode: FILE processing mode" << std::endl;
    std::vector<std::string> inputFileNames;
    for (int i = 1; i < argc; i++) {
      inputFileNames.push_back(argv[i]);
    }
    runAsFileProcessor(std::move(processor), inputFileNames);
  } else {
    std::cout << "Mode: SOCKET server mode" << std::endl;
    runAsSocketServer(std::move(processor));
  }

  return 0;
}