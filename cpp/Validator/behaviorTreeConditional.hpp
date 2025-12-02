#pragma once

#include "behaviorTreeResolvers.hpp"

class ConditionBase {
public:
  virtual ~ConditionBase() = default;
  virtual bool evaluate(const CurrentLine& currLine, const BehaviorTreeState& state) const = 0;
};

class ConditionLogicalAnd : public ConditionBase {
public:
  ConditionLogicalAnd(std::unique_ptr<ConditionBase> lhs, std::unique_ptr<ConditionBase> rhs) : 
    lhs_(std::move(lhs)), 
    rhs_(std::move(rhs)) {}

  bool evaluate(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    return (lhs_->evaluate(currLine, state) && (rhs_->evaluate(currLine, state)));
  }

private:
  std::unique_ptr<ConditionBase> lhs_;
  std::unique_ptr<ConditionBase> rhs_;
};

template<class T>
class ConditionScalarEquality : public ConditionBase {
public:
  ConditionScalarEquality(std::unique_ptr<CanResolveTo<ScalarVal<T>>> lhs, std::unique_ptr<CanResolveTo<ScalarVal<T>>> rhs, bool printOnFail = false) : lhs_(std::move(lhs)), rhs_(std::move(rhs)), printOnFail_(printOnFail) {}

  bool evaluate(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    const auto& lhs = lhs_->resolve(currLine, state).data;
    const auto& rhs = rhs_->resolve(currLine, state).data;
    // printf("lhs: %s, rhs: %s\n", lhs.c_str(), rhs.c_str());

    if (printOnFail_ && (lhs != rhs)) {
      printf("Condition mismatch | lhs: [%s] | rhs: [%s]\n", lhs.c_str(), rhs.c_str());
    }
    
    return lhs == rhs;
  }
private:
  std::unique_ptr<CanResolveTo<ScalarVal<T>>> lhs_;
  std::unique_ptr<CanResolveTo<ScalarVal<T>>> rhs_;
  bool printOnFail_;
};