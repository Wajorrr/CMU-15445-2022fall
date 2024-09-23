#include <algorithm>
#include <memory>
#include <unordered_map>

#include "binder/bound_expression.h"
#include "binder/statement/delete_statement.h"
#include "binder/statement/insert_statement.h"
#include "binder/statement/select_statement.h"
#include "binder/statement/update_statement.h"
#include "binder/tokens.h"
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/delete_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/insert_plan.h"
#include "execution/plans/update_plan.h"
#include "execution/plans/values_plan.h"
#include "planner/planner.h"
#include "type/type_id.h"

namespace bustub {

auto Planner::PlanInsert(const InsertStatement &statement) -> AbstractPlanNodeRef {
  // 通过调用 PlanSelect 函数生成一个选择计划节点
  auto select = PlanSelect(*statement.select_);

  // 获取目标表的模式和选择计划节点的输出模式，并比较两者的列类型是否一致
  const auto &table_schema = statement.table_->schema_.GetColumns();
  const auto &child_schema = select->OutputSchema().GetColumns();
  // 如果不一致，则抛出一个异常
  if (!std::equal(table_schema.cbegin(), table_schema.cend(), child_schema.cbegin(), child_schema.cend(),
                  [](auto &&col1, auto &&col2) { return col1.GetType() == col2.GetType(); })) {
    throw bustub::Exception("table schema mismatch");
  }

  // 创建一个新的插入计划节点，并返回该节点
  auto insert_schema = std::make_shared<Schema>(std::vector{Column("__bustub_internal.insert_rows", TypeId::INTEGER)});

  return std::make_shared<InsertPlanNode>(std::move(insert_schema), std::move(select), statement.table_->oid_);
}

auto Planner::PlanDelete(const DeleteStatement &statement) -> AbstractPlanNodeRef {
  // 通过 PlanTableRef 函数生成一个表引用计划节点
  auto table = PlanTableRef(*statement.table_);
  // 通过 PlanExpression 函数生成一个条件表达式
  auto [_, condition] = PlanExpression(*statement.expr_, {table});
  // 创建一个过滤计划节点，并将表引用计划节点和条件表达式传递给它
  auto filter = std::make_shared<FilterPlanNode>(table->output_schema_, std::move(condition), std::move(table));
  auto delete_schema = std::make_shared<Schema>(std::vector{Column("__bustub_internal.delete_rows", TypeId::INTEGER)});
  // 创建一个新的删除计划节点，并返回该节点
  return std::make_shared<DeletePlanNode>(std::move(delete_schema), std::move(filter), statement.table_->oid_);
}

auto Planner::PlanUpdate(const UpdateStatement &statement) -> AbstractPlanNodeRef {
  // 通过 PlanTableRef 函数生成一个表引用计划节点
  auto table = PlanTableRef(*statement.table_);
  // 通过 PlanExpression 函数生成一个条件表达式，并创建一个过滤计划节点
  auto [_, condition] = PlanExpression(*statement.filter_expr_, {table});
  AbstractPlanNodeRef filter =
      std::make_shared<FilterPlanNode>(table->output_schema_, std::move(condition), std::move(table));

  // 初始化一个作用域向量，并创建一个目标表达式向量
  auto scope = std::vector{filter};

  std::vector<AbstractExpressionRef> target_exprs;
  target_exprs.resize(filter->output_schema_->GetColumnCount());
  // 对于每个目标表达式，通过 PlanExpression 和 PlanColumnRef 函数生成相应的抽象表达式
  // 并将其存储在目标表达式向量中
  for (const auto &[col, target_expr] : statement.target_expr_) {
    auto [_1, target_abstract_expr] = PlanExpression(*target_expr, scope);
    auto [_2, col_abstract_expr] = PlanColumnRef(*col, scope);
    target_exprs[col_abstract_expr->GetColIdx()] = std::move(target_abstract_expr);
  }
  // 如果某个目标表达式为空，则创建一个列值表达式来填充它
  for (size_t idx = 0; idx < target_exprs.size(); idx++) {
    if (target_exprs[idx] == nullptr) {
      target_exprs[idx] =
          std::make_shared<ColumnValueExpression>(0, idx, filter->output_schema_->GetColumn(idx).GetType());
    }
  }

  auto update_schema = std::make_shared<Schema>(std::vector{Column("__bustub_internal.update_rows", TypeId::INTEGER)});
  // 创建一个新的更新计划节点，并返回该节点
  return std::make_shared<UpdatePlanNode>(std::move(update_schema), std::move(filter), statement.table_->oid_,
                                          std::move(target_exprs));
}

}  // namespace bustub
