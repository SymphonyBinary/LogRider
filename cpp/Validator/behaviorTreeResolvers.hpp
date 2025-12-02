#pragma once

#include "behaviorTreeData.hpp"
#include "behaviorTreeValues.hpp"

#include "json.hpp"

template <class T>
class CanResolveTo {
public:
  using ResolveToType = T;
  virtual ~CanResolveTo() = default;
  virtual ResolveToType resolve(const CurrentLine& currLine, const BehaviorTreeState& state) const = 0;
  
  virtual nlohmann::json toJson() {nlohmann::json j; return j;};
};

class LiteralString : public CanResolveTo<ScalarVal<std::string>> {
public:
  LiteralString(std::string literal) : literal{literal} {}

  ScalarVal<std::string> resolve(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    // printf("LiteralString::resolve: %s\n", literal.c_str());
    return ScalarVal<std::string>{literal};
  }

  nlohmann::json toJson() override {
    nlohmann::json j{{"typeid", "LiteralString"}, {"literal", literal}};
    return j;
  };
  
private:
  std::string literal;
};

class FromScanLine : public CanResolveTo<ScalarVal<std::string>> {
public:
  enum ScanLinePart {
    Label,
    Key,
    Value
  };

  FromScanLine(ScanLinePart type, int i = 0) : type_{type}, index_{i} {}

  ScalarVal<std::string> resolve(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    // printf("FromScanLine::resolve: full line: [%s]\n", currLine.node.messageText.innerPayload.c_str());

    ScalarVal<std::string> ret;
    
    switch (type_) {
      case Label:
        ret.data = currLine.scanLine.label;
        break;
      case Key:
        if (currLine.scanLine.kvps.size() > index_) {
          ret.data = currLine.scanLine.kvps[index_].key;
        }
        break;
      case Value:
        if (currLine.scanLine.kvps.size() > index_) {
          ret.data = currLine.scanLine.kvps[index_].value;
        }
        break;
      default:
        break;
    }

    // printf("FromScanLine::resolve: ret: [%s]\n", ret.data.c_str());
    return ret;
  }
private:
  ScanLinePart type_;
  int index_;
};

class FromStackNode : public CanResolveTo<ScalarVal<size_t>> {
public:
  enum StackNodePart {
    ProcessId
  };

  FromStackNode(StackNodePart type) : type_{type} {}

  ScalarVal<size_t> resolve(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    ScalarVal<size_t> ret;
    ret.data = currLine.node.uniqueProcessId;
    (void)type_;
    return ret;
  }

private:
  StackNodePart type_;
};

template<class I, class O>
class QueryStateBase : public CanResolveTo<O> {
public:
  using InType = I;
  using OutType = O;
};

class QueryStateGetObjectClassFromId : public QueryStateBase<ScalarVal<std::string>, ScalarVal<std::string>> {
public:
  QueryStateGetObjectClassFromId(std::unique_ptr<CanResolveTo<ScalarVal<std::string>>> objectId, 
                                 std::unique_ptr<CanResolveTo<ScalarVal<size_t>>> processId) : objectId_(std::move(objectId)), processId_(std::move(processId)) {}

  ScalarVal<std::string> resolve(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    ScalarVal<std::string> ret;

    size_t pId = processId_->resolve(currLine, state).data;
    std::string oId = objectId_->resolve(currLine, state).data;

    if (const auto processStateIt = state.processIdToState.find(pId); processStateIt != state.processIdToState.end()) {
      const auto& processState = processStateIt->second;
      if (const auto objectIt = processState.idToObjects.find(oId); objectIt != processState.idToObjects.end()) {
        ret.data = objectIt->second.back().className;
      }
    }

    return ret;
  }
private:
  std::unique_ptr<CanResolveTo<ScalarVal<std::string>>> objectId_;
  std::unique_ptr<CanResolveTo<ScalarVal<size_t>>> processId_;
};