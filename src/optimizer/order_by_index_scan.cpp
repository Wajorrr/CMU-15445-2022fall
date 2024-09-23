#include <algorithm>
#include <memory>

#include "binder/bound_order_by.h"
#include "catalog/catalog.h"
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "execution/plans/sort_plan.h"
#include "optimizer/optimizer.h"
#include "type/type_id.h"

namespace bustub {

// 用于优化查询计划，将排序操作转换为索引扫描操作
auto Optimizer::OptimizeOrderByAsIndexScan(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  std::vector<AbstractPlanNodeRef> children;
  // 递归地优化所有子节点，并将优化后的子节点克隆到当前计划节点中
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeOrderByAsIndexScan(child));
  }
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  // 函数检查优化后的计划节点的类型是否为 Sort
  // 如果是，则将其转换为 SortPlanNode 类型，并获取排序的列信息
  if (optimized_plan->GetType() == PlanType::Sort) {
    const auto &sort_plan = dynamic_cast<const SortPlanNode &>(*optimized_plan);
    const auto &order_bys = sort_plan.GetOrderBy();

    // Has exactly one order by column
    // 首先检查排序列的数量是否为 1，不是，则返回
    if (order_bys.size() != 1) {
      return optimized_plan;
    }

    // Order type is asc or default
    // 检查排序类型是否为升序（ASC）或默认排序（DEFAULT），不是，则返回
    const auto &[order_type, expr] = order_bys[0];
    if (!(order_type == OrderByType::ASC || order_type == OrderByType::DEFAULT)) {
      return optimized_plan;
    }

    // Order expression is a column value expression
    // 检查排序表达式是否为 ColumnValueExpression 类型，不是，则返回
    const auto *column_value_expr = dynamic_cast<ColumnValueExpression *>(expr.get());
    if (column_value_expr == nullptr) {
      return optimized_plan;
    }

    auto order_by_column_id = column_value_expr->GetColIdx();

    // Has exactly one child
    // 确保排序计划只有一个子节点
    BUSTUB_ENSURE(optimized_plan->children_.size() == 1, "Sort with multiple children?? Impossible!");
    const auto &child_plan = optimized_plan->children_[0];

    // 检查子计划的类型是否为 SeqScan
    if (child_plan->GetType() == PlanType::SeqScan) {
      const auto &seq_scan = dynamic_cast<const SeqScanPlanNode &>(*child_plan);
      const auto *table_info = catalog_.GetTable(seq_scan.GetTableOid());
      const auto indices = catalog_.GetTableIndexes(table_info->name_);

      // 遍历表的所有索引，检查索引的列是否与排序列匹配
      // 如果找到匹配的索引，则返回一个新的 IndexScanPlanNode，替代排序计划
      for (const auto *index : indices) {
        const auto &columns = index->key_schema_.GetColumns();
        if (columns.size() == 1 &&
            columns[0].GetName() == table_info->schema_.GetColumn(order_by_column_id).GetName()) {
          // Index matched, return index scan instead
          return std::make_shared<IndexScanPlanNode>(optimized_plan->output_schema_, index->index_oid_);
        }
      }
    }
  }

  return optimized_plan;
}

}  // namespace bustub
