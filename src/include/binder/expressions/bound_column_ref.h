#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "binder/bound_expression.h"
#include "common/macros.h"

namespace bustub {

/**
 * A bound column reference, e.g., `y.x` in the SELECT list.
 */
// 用于表示绑定的列引用，例如在 SQL 查询的 SELECT 列表中引用的列 y.x
class BoundColumnRef : public BoundExpression {
 public:
  explicit BoundColumnRef(std::vector<std::string> col_name)
      : BoundExpression(ExpressionType::COLUMN_REF), col_name_(std::move(col_name)) {}

  // 为列名添加前缀
  static auto Prepend(std::unique_ptr<BoundColumnRef> self, std::string prefix) -> std::unique_ptr<BoundColumnRef> {
    if (self == nullptr) {
      return nullptr;
    }
    // 创建一个新的变量 col_name，并将 prefix 移动到该变量中
    std::vector<std::string> col_name{std::move(prefix)};
    // 将 self 的列名复制到新的 col_name 中，并返回一个新的 BoundColumnRef 对象
    std::copy(self->col_name_.cbegin(), self->col_name_.cend(), std::back_inserter(col_name));
    return std::make_unique<BoundColumnRef>(std::move(col_name));
  }

  // 使用 fmt::join 函数将列名向量中的元素连接成一个字符串，并用点号（.）分隔
  auto ToString() const -> std::string override { return fmt::format("{}", fmt::join(col_name_, ".")); }

  auto HasAggregation() const -> bool override { return false; }

  /** The name of the column. */
  std::vector<std::string> col_name_;
};
}  // namespace bustub
