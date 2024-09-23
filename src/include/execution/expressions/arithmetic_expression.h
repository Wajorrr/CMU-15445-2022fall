//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arithmetic_expression.h
//
// Identification: src/include/expression/arithmetic_expression.h
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "catalog/schema.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/expressions/abstract_expression.h"
#include "fmt/format.h"
#include "storage/table/tuple.h"
#include "type/type_id.h"
#include "type/value_factory.h"

namespace bustub {

/** ArithmeticType represents the type of computation that we want to perform. */
// 包括加法（Plus）和减法（Minus）
enum class ArithmeticType { Plus, Minus };

/**
 * ArithmeticExpression represents two expressions being computed, ONLY SUPPORT INTEGER FOR NOW.
 */
// 用于表示两个表达式之间的算术运算
class ArithmeticExpression : public AbstractExpression {
 public:
  /** Creates a new comparison expression representing (left comp_type right). */
  ArithmeticExpression(AbstractExpressionRef left, AbstractExpressionRef right, ArithmeticType compute_type)
      : AbstractExpression({std::move(left), std::move(right)}, TypeId::INTEGER), compute_type_{compute_type} {
    // 检查两个操作数的返回类型是否为 INTEGER
    if (GetChildAt(0)->GetReturnType() != TypeId::INTEGER || GetChildAt(1)->GetReturnType() != TypeId::INTEGER) {
      throw bustub::NotImplementedException("only support integer for now");
    }
  }

  // 用于评估表达式。
  // 接收一个元组和一个模式作为参数，分别评估左操作数和右操作数
  // 然后调用 PerformComputation 方法进行计算
  auto Evaluate(const Tuple *tuple, const Schema &schema) const -> Value override {
    Value lhs = GetChildAt(0)->Evaluate(tuple, schema);
    Value rhs = GetChildAt(1)->Evaluate(tuple, schema);
    auto res = PerformComputation(lhs, rhs);
    if (res == std::nullopt) {
      return ValueFactory::GetNullValueByType(TypeId::INTEGER);
    }
    return ValueFactory::GetIntegerValue(*res);
  }

  // 用于评估连接操作中的表达式。
  // 接收两个元组和两个模式作为参数，分别评估左操作数和右操作数
  // 然后调用 PerformComputation 方法进行计算
  auto EvaluateJoin(const Tuple *left_tuple, const Schema &left_schema, const Tuple *right_tuple,
                    const Schema &right_schema) const -> Value override {
    // GetChildAt(0) 和 GetChildAt(1) 分别返回左子表达式和右子表达式
    // 然后调用它们的 EvaluateJoin 方法，传入相同的参数
    Value lhs = GetChildAt(0)->EvaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
    Value rhs = GetChildAt(1)->EvaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
    auto res = PerformComputation(lhs, rhs);
    if (res == std::nullopt) {
      return ValueFactory::GetNullValueByType(TypeId::INTEGER);
    }
    return ValueFactory::GetIntegerValue(*res);
  }

  /** @return the string representation of the expression node and its children */
  auto ToString() const -> std::string override {
    return fmt::format("({}{}{})", *GetChildAt(0), compute_type_, *GetChildAt(1));
  }

  BUSTUB_EXPR_CLONE_WITH_CHILDREN(ArithmeticExpression);

  ArithmeticType compute_type_;

 private:
  // 根据 compute_type_ 成员变量的值执行相应的算术运算
  // 接收两个 Value 对象作为参数，并返回一个 std::optional<int32_t> 类型的结果
  auto PerformComputation(const Value &lhs, const Value &rhs) const -> std::optional<int32_t> {
    if (lhs.IsNull() || rhs.IsNull()) {
      return std::nullopt;
    }
    switch (compute_type_) {
      case ArithmeticType::Plus:
        return lhs.GetAs<int32_t>() + rhs.GetAs<int32_t>();
      case ArithmeticType::Minus:
        return lhs.GetAs<int32_t>() - rhs.GetAs<int32_t>();
      default:
        UNREACHABLE("Unsupported arithmetic type.");
    }
  }
};
}  // namespace bustub

template <>
struct fmt::formatter<bustub::ArithmeticType> : formatter<string_view> {
  template <typename FormatContext>
  auto format(bustub::ArithmeticType c, FormatContext &ctx) const {
    string_view name;
    switch (c) {
      case bustub::ArithmeticType::Plus:
        name = "+";
        break;
      case bustub::ArithmeticType::Minus:
        name = "-";
        break;
      default:
        name = "Unknown";
        break;
    }
    return formatter<string_view>::format(name, ctx);
  }
};
