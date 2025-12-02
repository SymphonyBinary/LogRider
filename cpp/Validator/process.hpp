#pragma once

#include "ingest.hpp"
#include "behaviorTree.hpp"

struct Processor {
  Processor(std::ofstream&& outputStream, BehaviorTree* tree = nullptr) : validationTree(tree) {
    output.outputFileStream = std::move(outputStream);
  }

  void readLine(const std::string& line) {
    ProcessCaplogLine(line, worldWorkingData, worldState);
  }

  void printOutputIfAvailable() {
    auto& stackNodeArray = worldState.getNodeArray();

    while (stackNodeArray.size() > mSizePrinted ) {
      const auto& node = *stackNodeArray[mSizePrinted].get();

      if ((node.logLineType == CapLogType::BLOCK_CONCAT_BEGIN) || (node.logLineType == CapLogType::BLOCK_CONCAT_CONTINUE) || (node.logLineType == CapLogType::BLOCK_CONCAT_END)) {
        break;
      }

      output.outputFileStream
      << "P=" << node.uniqueProcessId
      << " T=" << node.uniqueThreadId
      << " C=" << node.commonLogText.channelId
      << " " << node.commonLogText.indentation
      << " " << node.commonLogText.functionId
      << " [" << node.commonLogText.sourceFileLine << "]";

      switch (node.logLineType) {
        case CapLogType::BLOCK_SCOPE_OPEN:
          // intentional fall through
        case CapLogType::BLOCK_SCOPE_CLOSE:
          output.outputFileStream << "::["
            << node.blockText.filename << "]::["
            << node.blockText.functionName << "] "
            << node.blockText.objectId;
          break;
        case CapLogType::BLOCK_INNER_LINE:
          output.outputFileStream << " "
            << node.messageText.innerTypeString
            << ": " << node.messageText.innerPayload;
          break;
        default:
          std::cerr << "CapLogType::UNKNOWN line" << std::endl;
          return;
          // std::abort();
      }
      
      ++mSizePrinted;
      output.outputFileStream << std::endl;

      if (validationTree) {
        validationTree->state.lineIndex = worldWorkingData.intputFileLineNumber;
        validationTree->execute(node, validationTree->state);
      }

      ++worldWorkingData.intputFileLineNumber;
    }
  }

  int mSizePrinted = 0;

  OutputState output;
  WorldState worldState;
  WorldStateWorkingData worldWorkingData;

  BehaviorTree* validationTree = nullptr;
};