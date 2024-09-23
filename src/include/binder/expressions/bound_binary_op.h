#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "binder/bound_expression.h"

namespace bustub {

/**
 * A bound binary operator, e.g., `a+b`.
 */
// 用于表示绑定的二元操作符
class BoundBinaryOp : public BoundExpression {
 public:
  explicit BoundBinaryOp(std::string op_name, std::unique_ptr<BoundExpression> larg,
                         std::unique_ptr<BoundExpression> rarg)
      : BoundExpression(ExpressionType::BINARY_OP),
        op_name_(std::move(op_name)),
        larg_(std::move(larg)),
        rarg_(std::move(rarg)) {}

  auto ToString() const -> std::string override { return fmt::format("({}{}{})", larg_, op_name_, rarg_); }

  auto HasAggregation() const -> bool override { return larg_->HasAggregation() || rarg_->HasAggregation(); }

  /** Operator name. */
  // 操作符名称
  std::string op_name_;

  /** Left argument of the op. */
  // 操作符的左操作数
  std::unique_ptr<BoundExpression> larg_;

  /** Right argument of the op. */
  // 操作符的右操作数
  std::unique_ptr<BoundExpression> rarg_;
};
}  // namespace bustub
