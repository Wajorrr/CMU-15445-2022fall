#include "execution/plans/limit_plan.h"
#include "execution/plans/sort_plan.h"
#include "execution/plans/topn_plan.h"
#include "optimizer/optimizer.h"

namespace bustub {

// 用于优化查询计划中的排序和限制操作，将其转换为 Top-N 操作
// 遍历查询计划树，识别排序和限制操作，并将其合并为 Top-N 操作，以提高查询性能
auto Optimizer::OptimizeSortLimitAsTopN(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement sort + limit -> top N optimizer rule
  std::vector<AbstractPlanNodeRef> children;
  // 遍历查询计划的子节点，并递归调用 OptimizeSortLimitAsTopN 方法对每个子节点进行优化
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeSortLimitAsTopN(child));
  }
  // 克隆当前查询计划节点，并将优化后的子节点作为其子节点
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  // 检查当前查询计划节点是否为限制操作（Limit）
  if (optimized_plan->GetType() == PlanType::Limit) {
    // 获取限制操作的具体信息，包括限制的行数 limit
    const auto &limit_plan = dynamic_cast<const LimitPlanNode &>(*optimized_plan);
    const auto &limit = limit_plan.GetLimit();

    // 同时，确保限制操作有且仅有一个子节点
    BUSTUB_ENSURE(limit_plan.children_.size() == 1, "Limit Plan should have exactly 1 child.");
    // 如果限制操作的子节点是排序操作（Sort）
    // 获取排序操作的具体信息，包括排序条件 order_bys
    if (optimized_plan->GetChildAt(0)->GetType() == PlanType::Sort) {
      const auto &sort_plan = dynamic_cast<const SortPlanNode &>(*optimized_plan->GetChildAt(0));
      const auto &order_bys = sort_plan.GetOrderBy();

      BUSTUB_ENSURE(sort_plan.children_.size() == 1, "Sort Plan should have exactly 1 child.");
      // 将限制操作和排序操作合并为一个 Top-N 操作，并返回新的 Top-N 查询计划节点
      return std::make_shared<TopNPlanNode>(limit_plan.output_schema_, sort_plan.GetChildAt(0), order_bys, limit);
    }
  }
  return optimized_plan;
}

}  // namespace bustub
