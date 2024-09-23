#include "optimizer/optimizer.h"
#include <optional>
#include "common/util/string_util.h"
#include "execution/plans/abstract_plan.h"

namespace bustub {

// 接收一个 AbstractPlanNodeRef 对象作为参数，并返回一个优化后的 AbstractPlanNodeRef 对象
auto Optimizer::Optimize(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // 首先检查成员变量 force_starter_rule_ 是否为真
  if (force_starter_rule_) {
    // Use starter rules when `force_starter_rule_` is set to true.
    // 为真，则依次应用一系列初始优化规则
    auto p = plan;
    p = OptimizeMergeProjection(p);
    p = OptimizeMergeFilterNLJ(p);
    p = OptimizeNLJAsIndexJoin(p);
    p = OptimizeOrderByAsIndexScan(p);
    p = OptimizeSortLimitAsTopN(p);
    return p;
  }
  // By default, use user-defined rules.
  // 否则，函数将调用 OptimizeCustom 函数，使用用户定义的规则进行优化
  return OptimizeCustom(plan);
}

// 用于估计表的基数（即表中的行数）
auto Optimizer::EstimatedCardinality(const std::string &table_name) -> std::optional<size_t> {
  // 如果表名以 "_1m" 结尾，则返回 1000000，以此类推
  if (StringUtil::EndsWith(table_name, "_1m")) {
    return std::make_optional(1000000);
  }
  if (StringUtil::EndsWith(table_name, "_100k")) {
    return std::make_optional(100000);
  }
  if (StringUtil::EndsWith(table_name, "_50k")) {
    return std::make_optional(50000);
  }
  if (StringUtil::EndsWith(table_name, "_10k")) {
    return std::make_optional(10000);
  }
  if (StringUtil::EndsWith(table_name, "_1k")) {
    return std::make_optional(1000);
  }
  if (StringUtil::EndsWith(table_name, "_100")) {
    return std::make_optional(100);
  }
  // 如果表名不符合任何已知的后缀模式，则返回 std::nullopt，表示无法估计基数
  return std::nullopt;
}

}  // namespace bustub
