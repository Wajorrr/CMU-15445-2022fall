#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "binder/bound_expression.h"
#include "binder/bound_order_by.h"
#include "binder/bound_statement.h"
#include "binder/bound_table_ref.h"
#include "binder/expressions/bound_constant.h"
#include "binder/statement/insert_statement.h"
#include "binder/statement/select_statement.h"
#include "binder/tokens.h"
#include "catalog/schema.h"
#include "common/enums/statement_type.h"
#include "common/exception.h"
#include "common/macros.h"
#include "common/util/string_util.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/aggregation_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/limit_plan.h"
#include "execution/plans/projection_plan.h"
#include "execution/plans/sort_plan.h"
#include "execution/plans/values_plan.h"
#include "fmt/format.h"
#include "planner/planner.h"
#include "type/type_id.h"
#include "type/value_factory.h"

namespace bustub {

// 用于将 SelectStatement 转换为一个 AbstractPlanNodeRef 类型的计划节点
auto Planner::PlanSelect(const SelectStatement &statement) -> AbstractPlanNodeRef {
  // 上下文保护对象 ctx_guard，确保在方法执行期间上下文的一致性
  auto ctx_guard = NewContext();
  // 如果 SelectStatement 包含公共表表达式（CTE），则将其添加到当前上下文中
  if (!statement.ctes_.empty()) {
    ctx_.cte_list_ = &statement.ctes_;
  }

  AbstractPlanNodeRef plan = nullptr;

  // 根据 SelectStatement 中表引用的类型，选择不同的计划节点
  switch (statement.table_->type_) {
    case TableReferenceType::EMPTY:
      // 表引用类型为空（TableReferenceType::EMPTY）
      // 则创建一个 ValuesPlanNode，其模式为空，表达式列表也为空
      plan = std::make_shared<ValuesPlanNode>(
          std::make_shared<Schema>(std::vector<Column>{}),
          std::vector<std::vector<AbstractExpressionRef>>{std::vector<AbstractExpressionRef>{}});
      break;
    default:
      // 否则，调用 PlanTableRef 方法来处理表引用
      plan = PlanTableRef(*statement.table_);
      break;
  }

  // 包含 WHERE 子句，则为其创建一个 FilterPlanNode
  if (!statement.where_->IsInvalid()) {
    // 首先获取当前计划节点的输出模式
    auto schema = plan->OutputSchema();
    // 然后调用 PlanExpression 方法生成过滤表达式
    auto [_, expr] = PlanExpression(*statement.where_, {plan});
    // 最后创建一个新的 FilterPlanNode
    plan = std::make_shared<FilterPlanNode>(std::make_shared<Schema>(schema), std::move(expr), std::move(plan));
  }

  // 检查 SelectStatement 中是否包含聚合函数
  bool has_agg = false;
  for (const auto &item : statement.select_list_) {
    if (item->HasAggregation()) {
      has_agg = true;
      break;
    }
  }

  // 如果存在聚合函数、HAVING 子句或 GROUP BY 子句，则调用 PlanSelectAgg 方法来处理聚合查询
  if (!statement.having_->IsInvalid() || !statement.group_by_.empty() || has_agg) {
    // Plan aggregation
    plan = PlanSelectAgg(statement, std::move(plan));
  } else {  // 否则，处理普通的 SELECT 查询
    // Plan normal select
    std::vector<AbstractExpressionRef> exprs;
    std::vector<std::string> column_names;
    std::vector<AbstractPlanNodeRef> children = {plan};
    // 为每个选择列表中的项调用 PlanExpression 方法生成表达式
    // 并创建一个 ProjectionPlanNode
    for (const auto &item : statement.select_list_) {
      auto [name, expr] = PlanExpression(*item, {plan});
      if (name == UNNAMED_COLUMN) {
        name = fmt::format("__unnamed#{}", universal_id_++);
      }
      // 将表达式和列名添加到对应的列表中
      exprs.emplace_back(std::move(expr));
      column_names.emplace_back(std::move(name));
    }
    // 创建 ProjectionPlanNode
    plan = std::make_shared<ProjectionPlanNode>(std::make_shared<Schema>(ProjectionPlanNode::RenameSchema(
                                                    ProjectionPlanNode::InferProjectionSchema(exprs), column_names)),
                                                std::move(exprs), std::move(plan));
  }

  // Plan DISTINCT as group agg
  // 如果 SelectStatement 包含 DISTINCT 关键字
  // 则创建一个 AggregationPlanNode 来处理去重操作
  if (statement.is_distinct_) {
    auto child = std::move(plan);

    std::vector<AbstractExpressionRef> distinct_exprs;
    size_t col_idx = 0;
    for (const auto &col : child->OutputSchema().GetColumns()) {
      // 为每一列创建一个 ColumnValueExpression
      distinct_exprs.emplace_back(std::make_shared<ColumnValueExpression>(0, col_idx++, col.GetType()));
    }

    plan = std::make_shared<AggregationPlanNode>(std::make_shared<Schema>(child->OutputSchema()), child,
                                                 std::move(distinct_exprs), std::vector<AbstractExpressionRef>{},
                                                 std::vector<AggregationType>{});
  }

  // Plan ORDER BY
  // 如果 SelectStatement 包含 ORDER BY 子句，则创建一个 SortPlanNode
  if (!statement.sort_.empty()) {
    std::vector<std::pair<OrderByType, AbstractExpressionRef>> order_bys;
    // 为每个排序项调用 PlanExpression 方法生成排序表达式
    // 并将其添加到排序计划节点中
    for (const auto &order_by : statement.sort_) {
      auto [_, expr] = PlanExpression(*order_by->expr_, {plan});
      auto abstract_expr = std::move(expr);
      // 将排序类型和表达式添加到排序列表中
      order_bys.emplace_back(std::make_pair(order_by->type_, abstract_expr));
    }
    plan = std::make_shared<SortPlanNode>(std::make_shared<Schema>(plan->OutputSchema()), plan, std::move(order_bys));
  }

  // Plan LIMIT
  // 如果 SelectStatement 包含 LIMIT 或 OFFSET 子句，则创建一个 LimitPlanNode
  if (!statement.limit_count_->IsInvalid() || !statement.limit_offset_->IsInvalid()) {
    // 检查 LIMIT 和 OFFSET 子句是否为常量表达式，并将其转换为整数值
    std::optional<size_t> offset = std::nullopt;
    std::optional<size_t> limit = std::nullopt;

    if (!statement.limit_count_->IsInvalid()) {
      if (statement.limit_count_->type_ == ExpressionType::CONSTANT) {
        const auto &constant_expr = dynamic_cast<BoundConstant &>(*statement.limit_count_);
        const auto val = constant_expr.val_.CastAs(TypeId::INTEGER);
        if (constant_expr.val_.GetTypeId() == TypeId::INTEGER) {
          limit = std::make_optional(constant_expr.val_.GetAs<int32_t>());
        } else {
          throw NotImplementedException("LIMIT clause must be an integer constant.");
        }
      } else {
        throw NotImplementedException("LIMIT clause must be an integer constant.");
      }
    }

    // 如果 OFFSET 子句存在，则抛出未实现异常，因为当前不支持 OFFSET 子句
    if (!statement.limit_offset_->IsInvalid()) {
      if (statement.limit_offset_->type_ == ExpressionType::CONSTANT) {
        const auto &constant_expr = dynamic_cast<BoundConstant &>(*statement.limit_offset_);
        const auto val = constant_expr.val_.CastAs(TypeId::INTEGER);
        if (constant_expr.val_.GetTypeId() == TypeId::INTEGER) {
          offset = std::make_optional(constant_expr.val_.GetAs<int32_t>());
        } else {
          throw NotImplementedException("OFFSET clause must be an integer constant.");
        }
      } else {
        throw NotImplementedException("OFFSET clause must be an integer constant.");
      }
    }

    if (offset != std::nullopt) {
      throw NotImplementedException("OFFSET clause is not supported yet.");
    }

    plan = std::make_shared<LimitPlanNode>(std::make_shared<Schema>(plan->OutputSchema()), plan, *limit);
  }

  return plan;
}

}  // namespace bustub
