#pragma once

#include <vector>

#include "ingest.hpp"
#include "behaviorTreeUtils.hpp"
#include "behaviorTreeResolvers.hpp"
#include "behaviorTreeConditional.hpp"

enum class NodeStatus {
  SUCCESS,
  EXECUTING, //implies success, but don't "close" the node
  FAILED,
  IGNORED,
};

class BehaviorNodeBase {
public:
  virtual ~BehaviorNodeBase() = default;
  virtual NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) = 0;
};

class BehaviorNodeSequence : public BehaviorNodeBase{
public:
  BehaviorNodeSequence(std::vector<std::unique_ptr<BehaviorNodeBase>>children) :
    children_(std::move(children)) {}

  NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) override {
    if (currentChild < children_.size()) {
      auto retStatus = children_[currentChild]->execute(currLine, state);
      switch (retStatus) {
        case NodeStatus::SUCCESS:
          ++currentChild;
          if (currentChild < children_.size()) {
            return NodeStatus::EXECUTING;
          } else {
            return NodeStatus::SUCCESS;
          }
        case NodeStatus::EXECUTING:
          return NodeStatus::EXECUTING;
        case NodeStatus::FAILED:
          return NodeStatus::FAILED;
        case NodeStatus::IGNORED:
          return NodeStatus::IGNORED;
      }      
    } else {
      return NodeStatus::SUCCESS;
    }
  }

private:
  std::vector<std::unique_ptr<BehaviorNodeBase>> children_;
  int currentChild = 0;
};

class BehaviorNodeRunAll : public BehaviorNodeBase {
public:
  BehaviorNodeRunAll(std::vector<std::unique_ptr<BehaviorNodeBase>> children) :
    children_(std::move(children)) {}

  NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) override {
    for (auto& child : children_) {
      NodeStatus retStatus = child->execute(currLine, state);
      if (retStatus == NodeStatus::FAILED) {
        return NodeStatus::FAILED;
      }
    }

    return NodeStatus::EXECUTING;
  }

private:
  std::vector<std::unique_ptr<BehaviorNodeBase>> children_;
};

class BehaviorNodeMemoObjectPointer : public BehaviorNodeBase {
public:
  NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) override {
    if (currLine.funcString.valid) {
      const FuncString& funcString = currLine.funcString;

      if (funcString.isCtor) {
        state.processIdToState[currLine.node.uniqueProcessId].idToObjects[currLine.node.blockText.objectId].push_back(ObjectData{funcString.name, currLine.node.uniqueProcessId, currLine.node.uniqueThreadId});
        
        if (state.validationOStream) {
          (*state.validationOStream) << "[Memo] | Line [" << state.lineIndex << "] | CTOR | ProcessId: [" << currLine.node.uniqueProcessId << "] | ObjectId: [" << currLine.node.blockText.objectId 
          << "] | FunctionName: [" << funcString.name << "] | ThreadId: [" << currLine.node.uniqueThreadId << "]" << std::endl;
          state.validationOStream->flush();
        }
      } else if (funcString.isDtor) {
        auto& objects = state.processIdToState[currLine.node.uniqueProcessId].idToObjects[currLine.node.blockText.objectId];
        for (auto it = objects.begin(); it != objects.end();) {
          if (it->className == funcString.className) {
            it = objects.erase(it);
          } else {
            ++it;
          }
        }

        if (state.validationOStream) {
          (*state.validationOStream) << "[Memo] | Line [" << state.lineIndex << "] | DTOR | ProcessId: [" << currLine.node.uniqueProcessId << "] | ObjectId: [" << currLine.node.blockText.objectId 
          << "] | FunctionName: [" << funcString.name << "] | ThreadId: [" << currLine.node.uniqueThreadId << "]" << std::endl;
          state.validationOStream->flush();
        }
      }
    }
    return NodeStatus::EXECUTING;
  }
};

class BehaviorNodeConditionalExecute : public BehaviorNodeBase {
public:
  BehaviorNodeConditionalExecute(std::unique_ptr<ConditionBase> triggerCondition, std::unique_ptr<BehaviorNodeBase> child) : 
    triggerCondition_(std::move(triggerCondition)), child_(std::move(child)) {}

  NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) override {
    if (triggerCondition_->evaluate(currLine, state)) {
      return child_->execute(currLine, state);
    }
    return NodeStatus::EXECUTING;
  }
private:
  std::unique_ptr<ConditionBase> triggerCondition_ = nullptr;
  std::unique_ptr<BehaviorNodeBase> child_ = nullptr;
};

class BehaviorNodeCheckMessage : public BehaviorNodeBase {
public:
  BehaviorNodeCheckMessage(std::unique_ptr<ConditionBase> check) : check_(std::move(check)) {}

  NodeStatus execute(const CurrentLine& currLine, BehaviorTreeState& state) override {
    bool result = check_->evaluate(currLine, state);
    
    if (state.validationOStream) {
      (*state.validationOStream) << "[Check] | Line [" << state.lineIndex << "] | TRIGGERED | Result: [" << (result ? std::string("SUCCESS") : std::string("FAIL"))
      << "] | ProcessId: [" << currLine.node.uniqueProcessId << "] | ThreadId: [" << currLine.node.uniqueThreadId 
      << "] | messageText: [" << currLine.node.messageText.innerPayload << "]" << std::endl;
      state.validationOStream->flush();
    }

    if (!result) {
      std::cout << "Check failed at line. " << state.lineIndex << std::endl;
      std::cout << currLine.node.messageText.innerPayload << std::endl;
    }

    return result ? NodeStatus::SUCCESS : NodeStatus::FAILED;
  }
private:
  std::unique_ptr<ConditionBase> check_ = nullptr;
};

struct BehaviorTree {
  NodeStatus execute(const StackNode& stackNode) {
    CurrentLine currLine {
      .node = stackNode,
      .funcString = getFuncName(stackNode),
      .scanLine = parseScanLine(stackNode),
    };

    return root.execute(currLine, state);
  }

  BehaviorNodeRunAll root;
  BehaviorTreeState state;
};