//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// constant_value_expression.h
//
// Identification: src/include/expression/constant_value_expression.h
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "execution/expressions/abstract_expression.h"

namespace bustub {
/**
 * ConstantValueExpression represents constants.
 */
// 用于表示常量值表达式
class ConstantValueExpression : public AbstractExpression {
 public:
  /** Creates a new constant value expression wrapping the given value. */
  // 接收一个 Value 对象，并将其包装为一个常量值表达式
  explicit ConstantValueExpression(const Value &val) : AbstractExpression({}, val.GetTypeId()), val_(val) {}

  // 用于返回常量值。它忽略传入的元组和模式参数，直接返回存储的常量值 val_
  auto Evaluate(const Tuple *tuple, const Schema &schema) const -> Value override { return val_; }

  // 用于在连接操作中返回常量值。它忽略传入的左右元组和模式参数，直接返回存储的常量值 val_
  auto EvaluateJoin(const Tuple *left_tuple, const Schema &left_schema, const Tuple *right_tuple,
                    const Schema &right_schema) const -> Value override {
    return val_;
  }

  /** @return the string representation of the plan node and its children */
  auto ToString() const -> std::string override { return val_.ToString(); }

  BUSTUB_EXPR_CLONE_WITH_CHILDREN(ConstantValueExpression);

  // 一个 Value 对象，表示常量值
  Value val_;
};
}  // namespace bustub
