#pragma once

#include "behaviorTreeResolvers.hpp"

class ConditionBase {
public:
  virtual ~ConditionBase() = default;
  virtual bool evaluate(const CurrentLine& currLine, const BehaviorTreeState& state) const = 0;
};

template<class T>
class ConditionScalarEquality : public ConditionBase {
public:
  ConditionScalarEquality(std::unique_ptr<CanResolveTo<ScalarVal<T>>> lhs, std::unique_ptr<CanResolveTo<ScalarVal<T>>> rhs) : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  bool evaluate(const CurrentLine& currLine, const BehaviorTreeState& state) const override {
    const auto& lhs = lhs_->resolve(currLine, state).data;
    const auto& rhs = rhs_->resolve(currLine, state).data;
    // printf("lhs: %s, rhs: %s\n", lhs.c_str(), rhs.c_str());
    return lhs == rhs;
  }
private:
  std::unique_ptr<CanResolveTo<ScalarVal<T>>> lhs_;
  std::unique_ptr<CanResolveTo<ScalarVal<T>>> rhs_;
};