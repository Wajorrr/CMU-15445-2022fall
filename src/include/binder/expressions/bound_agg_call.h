#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "binder/bound_expression.h"

namespace bustub {

/**
 * A bound aggregate call, e.g., `sum(x)`.
 */
// 用于表示绑定的聚合调用，例如 sum(x)
class BoundAggCall : public BoundExpression {
 public:
  explicit BoundAggCall(std::string func_name, bool is_distinct, std::vector<std::unique_ptr<BoundExpression>> args)
      : BoundExpression(ExpressionType::AGG_CALL),
        func_name_(std::move(func_name)),
        is_distinct_(is_distinct),
        args_(std::move(args)) {}

  auto ToString() const -> std::string override;

  auto HasAggregation() const -> bool override { return true; }

  /** Function name. */
  // 聚合函数名称
  std::string func_name_;

  /** Is distinct aggregation */
  // 聚合是否是 DISTINCT 聚合
  bool is_distinct_;

  /** Arguments of the agg call. */
  // 聚合函数的参数
  std::vector<std::unique_ptr<BoundExpression>> args_;
};
}  // namespace bustub
