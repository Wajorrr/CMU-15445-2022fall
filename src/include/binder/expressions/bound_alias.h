#pragma once

#include <memory>
#include <string>
#include <utility>
#include "binder/bound_expression.h"

namespace bustub {

/**
 * The alias in SELECT list, e.g. `SELECT count(x) AS y`, the `y` is an alias.
 */
// 用于表示 SELECT 列表中的别名，例如 SELECT count(x) AS y，y 就是一个别名
class BoundAlias : public BoundExpression {
 public:
  explicit BoundAlias(std::string alias, std::unique_ptr<BoundExpression> child)
      : BoundExpression(ExpressionType::ALIAS), alias_(std::move(alias)), child_(std::move(child)) {}

  auto ToString() const -> std::string override { return fmt::format("({} as {})", child_, alias_); }

  // 检查子表达式是否包含聚合。调用子表达式的 HasAggregation 方法，并返回结果
  auto HasAggregation() const -> bool override { return child_->HasAggregation(); }

  /** Alias name. */
  // 别名的名称
  std::string alias_;

  /** The actual expression */
  // 实际的表达式
  std::unique_ptr<BoundExpression> child_;
};
}  // namespace bustub
