#include <algorithm>
#include <memory>
#include "catalog/column.h"
#include "catalog/schema.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/projection_plan.h"
#include "optimizer/optimizer.h"

namespace bustub {

// 用于优化查询计划中的投影操作
// 会合并那些执行相同投影操作的节点，以提高查询执行效率
auto Optimizer::OptimizeMergeProjection(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // 创建一个空的 std::vector<AbstractPlanNodeRef> 容器来存储优化后的子节点
  std::vector<AbstractPlanNodeRef> children;
  // 遍历当前计划节点的所有子节点，并递归调用自身对每个子节点进行优化
  // 并将优化后的子节点添加到容器中
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeMergeProjection(child));
  }
  // 使用优化后的子节点克隆当前计划节点
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  // 检查克隆后的计划节点是否为投影类型（PlanType::Projection）
  if (optimized_plan->GetType() == PlanType::Projection) {
    // 是，则进一步检查该投影节点是否只有一个子节点
    // 并确保子节点的模式与投影节点的模式相同（除了列名）
    const auto &projection_plan = dynamic_cast<const ProjectionPlanNode &>(*optimized_plan);
    // Has exactly one child
    BUSTUB_ENSURE(optimized_plan->children_.size() == 1, "Projection with multiple children?? That's weird!");
    // If the schema is the same (except column name)
    const auto &child_plan = optimized_plan->children_[0];
    const auto &child_schema = child_plan->OutputSchema();
    const auto &projection_schema = projection_plan.OutputSchema();
    const auto &child_columns = child_schema.GetColumns();
    const auto &projection_columns = projection_schema.GetColumns();
    // 比较子节点和投影节点的列类型，如果所有列的类型都相同
    // 则继续检查投影节点中的所有表达式是否都是列值表达式
    // 并且这些表达式的列索引和元组索引是否与其在子节点中的位置一致
    if (std::equal(child_columns.begin(), child_columns.end(), projection_columns.begin(), projection_columns.end(),
                   [](auto &&child_col, auto &&proj_col) {
                     // TODO(chi): consider VARCHAR length
                     return child_col.GetType() == proj_col.GetType();
                   })) {
      const auto &exprs = projection_plan.GetExpressions();
      // If all items are column value expressions
      // 表达式的列索引和元组索引是否与其在子节点中的位置一致
      bool is_identical = true;
      for (size_t idx = 0; idx < exprs.size(); idx++) {
        auto column_value_expr = dynamic_cast<const ColumnValueExpression *>(exprs[idx].get());
        if (column_value_expr != nullptr) {
          if (column_value_expr->GetTupleIdx() == 0 && column_value_expr->GetColIdx() == idx) {
            continue;
          }
        }
        is_identical = false;
        break;
      }
      // 如果上述条件都满足，则说明该投影节点与其子节点执行的是相同的投影操作
      // 克隆子节点，并将投影节点的输出模式设置为子节点的输出模式，然后返回优化后的子节点
      if (is_identical) {
        auto plan = child_plan->CloneWithChildren(child_plan->GetChildren());
        plan->output_schema_ = std::make_shared<Schema>(projection_schema);
        return plan;
      }
    }
  }
  return optimized_plan;
}

}  // namespace bustub
