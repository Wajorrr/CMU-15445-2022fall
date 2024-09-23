#include <memory>
#include <tuple>
#include "binder/bound_expression.h"
#include "binder/bound_statement.h"
#include "binder/expressions/bound_agg_call.h"
#include "binder/expressions/bound_alias.h"
#include "binder/expressions/bound_binary_op.h"
#include "binder/expressions/bound_column_ref.h"
#include "binder/expressions/bound_constant.h"
#include "binder/expressions/bound_unary_op.h"
#include "binder/statement/select_statement.h"
#include "common/exception.h"
#include "common/macros.h"
#include "common/util/string_util.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "fmt/format.h"
#include "planner/planner.h"

namespace bustub {

// 用于处理二元操作表达式，并生成相应的抽象表达式引用
auto Planner::PlanBinaryOp(const BoundBinaryOp &expr, const std::vector<AbstractPlanNodeRef> &children)
    -> AbstractExpressionRef {
  // 调用 PlanExpression 方法来处理左子表达式 expr.larg_，并将结果存储在 left 变量中
  // 使用了结构化绑定语法，忽略了第一个返回值
  auto [_1, left] = PlanExpression(*expr.larg_, children);
  auto [_2, right] = PlanExpression(*expr.rarg_, children);
  // 获取二元操作符的名称 op_name
  const auto &op_name = expr.op_name_;
  // 调用 GetBinaryExpressionFromFactory 工厂函数
  // 传递操作符名称、左子表达式和右子表达式，生成并返回一个二元表达式对象
  return GetBinaryExpressionFromFactory(op_name, std::move(left), std::move(right));
}

// 为给定的列引用表达式和子计划节点生成一个 ColumnValueExpression 对象
// 两个参数：
// expr：一个 BoundColumnRef 对象，表示列引用表达式
// children：一个包含 AbstractPlanNodeRef 对象的向量，表示子计划节点
auto Planner::PlanColumnRef(const BoundColumnRef &expr, const std::vector<AbstractPlanNodeRef> &children)
    -> std::tuple<std::string, std::shared_ptr<ColumnValueExpression>> {
  // 检查 children 向量是否为空
  // 如果为空，则抛出异常，因为列引用表达式至少需要一个子计划节点
  if (children.empty()) {
    throw Exception("column ref should have at least one child");
  }

  auto col_name = expr.ToString();

  // 如果 children 向量的大小为 1，表示这是一个单子计划节点的情况，如投影、过滤等
  if (children.size() == 1) {
    // Projections, Filters, and other executors evaluating expressions with one single child will
    // use this branch.
    // 首先获取子计划节点的输出模式，并检查是否存在重复的列名
    const auto &child = children[0];
    auto schema = child->OutputSchema();
    // Before we can call `schema.GetColIdx`,  we need to ensure there's no duplicated column.
    bool found = false;
    for (const auto &col : schema.GetColumns()) {
      if (col_name == col.GetName()) {
        if (found) {
          throw bustub::Exception("duplicated column found in schema");
        }
        found = true;
      }
    }
    // 获取列的索引和类型，并返回一个包含列名和 ColumnValueExpression 对象的元组
    uint32_t col_idx = schema.GetColIdx(col_name);
    auto col_type = schema.GetColumn(col_idx).GetType();
    return std::make_tuple(col_name, std::make_shared<ColumnValueExpression>(0, col_idx, col_type));
  }
  if (children.size() == 2) {
    // 如果 children 向量的大小为 2，表示这是一个连接操作
    /*
     * Joins will use this branch to plan expressions.
     *
     * If an expression is for join condition, e.g.
     * SELECT * from test_1 inner join test_2 on test_1.colA = test_2.col2
     * The plan will be like:
     * ```
     * NestedLoopJoin condition={ ColumnRef 0.0=ColumnRef 1.1 }
     *   SeqScan colA, colB
     *   SeqScan col1, col2
     * ```
     * In `ColumnRef n.m`, when executor is using the expression, it picks from its
     * nth children's mth column to get the data.
     */
    // 分别获取左右子计划节点的输出模式，并尝试在左右模式中查找列名
    const auto &left = children[0];
    const auto &right = children[1];
    auto left_schema = left->OutputSchema();
    auto right_schema = right->OutputSchema();

    // 根据列名在左右模式中查找列索引(int)
    auto col_idx_left = left_schema.TryGetColIdx(col_name);
    auto col_idx_right = right_schema.TryGetColIdx(col_name);
    // 如果在左右模式中都找到列名，则抛出异常，表示列名不明确
    if (col_idx_left && col_idx_right) {
      throw bustub::Exception(fmt::format("ambiguous column name {}", col_name));
    }
    // 如果只在左模式中找到列名
    // 则返回一个包含列名和 ColumnValueExpression 对象的元组，表示从左子计划节点中获取数据
    if (col_idx_left) {
      auto col_type = left_schema.GetColumn(*col_idx_left).GetType();
      return std::make_tuple(col_name, std::make_shared<ColumnValueExpression>(0, *col_idx_left, col_type));
    }
    // 如果只在右模式中找到列名
    // 则返回一个包含列名和 ColumnValueExpression 对象的元组，表示从右子计划节点中获取数据
    if (col_idx_right) {
      auto col_type = right_schema.GetColumn(*col_idx_right).GetType();
      return std::make_tuple(col_name, std::make_shared<ColumnValueExpression>(1, *col_idx_right, col_type));
    }
    // 如果在左右模式中都找不到列名，则抛出异常
    throw bustub::Exception(fmt::format("column name {} not found", col_name));
  }
  // 当前不支持超过两个子计划节点的执行器
  UNREACHABLE("no executor with expression has more than 2 children for now");
}

// 主要作用是为给定的常量表达式生成一个 ConstantValueExpression 对象
auto Planner::PlanConstant(const BoundConstant &expr, const std::vector<AbstractPlanNodeRef> &children)
    -> AbstractExpressionRef {
  // 直接返回一个 ConstantValueExpression 对象的共享指针
  // 这个对象是通过传递 expr.val_（常量表达式的值）来构造的
  return std::make_shared<ConstantValueExpression>(expr.val_);
}

// 将聚合调用表达式添加到上下文中
void Planner::AddAggCallToContext(BoundExpression &expr) {
  switch (expr.type_) {
    // 对于聚合调用表达式（ExpressionType::AGG_CALL），函数首先将表达式转换为 BoundAggCall 类型
    case ExpressionType::AGG_CALL: {
      auto &agg_call_expr = dynamic_cast<BoundAggCall &>(expr);
      auto agg_name = fmt::format("__pseudo_agg#{}", ctx_.aggregations_.size());
      // 然后，它生成一个伪聚合名称，并创建一个新的 BoundAggCall 对象
      auto agg_call =
          BoundAggCall(agg_name, agg_call_expr.is_distinct_, std::vector<std::unique_ptr<BoundExpression>>{});
      // 使用 std::exchange 将原始的聚合调用替换为伪聚合调用，并将其添加到上下文中
      // Replace the agg call in the original bound expression with a pseudo one, add agg call to the context.
      // std::exchange用于交换两个对象的值
      ctx_.AddAggregation(std::make_unique<BoundAggCall>(std::exchange(agg_call_expr, std::move(agg_call))));
      return;
    }
    case ExpressionType::COLUMN_REF: {
      // 列引用表达式（ExpressionType::COLUMN_REF）不需要处理
      return;
    }
    case ExpressionType::BINARY_OP: {
      // 对于二元操作表达式（ExpressionType::BINARY_OP）
      // 函数将表达式转换为 BoundBinaryOp 类型
      // 并递归地对其左右子表达式调用 AddAggCallToContext 函数
      auto &binary_op_expr = dynamic_cast<BoundBinaryOp &>(expr);
      AddAggCallToContext(*binary_op_expr.larg_);
      AddAggCallToContext(*binary_op_expr.rarg_);
      return;
    }
    case ExpressionType::CONSTANT: {
      // 常量表达式（ExpressionType::CONSTANT）不需要处理
      return;
    }
    case ExpressionType::ALIAS: {
      // 对于别名表达式（ExpressionType::ALIAS）
      // 函数将表达式转换为 BoundAlias 类型
      // 并递归地对其子表达式调用 AddAggCallToContext 函数
      auto &alias_expr = dynamic_cast<const BoundAlias &>(expr);
      AddAggCallToContext(*alias_expr.child_);
      return;
    }
    default:
      break;
  }
  throw Exception(fmt::format("expression type {} not supported in planner yet", expr.type_));
}

// 用于将绑定的表达式转换为抽象表达式，并返回一个包含表达式名称和表达式引用的元组
// 接受一个 BoundExpression 对象和一个包含子计划节点的向量作为参数
auto Planner::PlanExpression(const BoundExpression &expr, const std::vector<AbstractPlanNodeRef> &children)
    -> std::tuple<std::string, AbstractExpressionRef> {
  switch (expr.type_) {
    case ExpressionType::AGG_CALL: {
      // 首先检查当前上下文中的聚合表达式是否已处理完毕
      if (ctx_.next_aggregation_ >= ctx_.expr_in_agg_.size()) {
        throw bustub::Exception("unexpected agg call");
      }
      // 如果没有，则从上下文中获取下一个聚合表达式，并返回一个包含未命名列和聚合表达式的元组
      return std::make_tuple(UNNAMED_COLUMN, std::move(ctx_.expr_in_agg_[ctx_.next_aggregation_++]));
    }
    case ExpressionType::COLUMN_REF: {
      // 将表达式转换为 BoundColumnRef 对象
      const auto &column_ref_expr = dynamic_cast<const BoundColumnRef &>(expr);
      // 调用 PlanColumnRef 方法来处理列引用表达式
      return PlanColumnRef(column_ref_expr, children);
    }
    case ExpressionType::BINARY_OP: {
      // 将表达式转换为 BoundBinaryOp 对象
      const auto &binary_op_expr = dynamic_cast<const BoundBinaryOp &>(expr);
      // 调用 PlanBinaryOp 方法来处理二元操作表达式
      return std::make_tuple(UNNAMED_COLUMN, PlanBinaryOp(binary_op_expr, children));
    }
    case ExpressionType::CONSTANT: {
      // 将表达式转换为 BoundConstant 对象
      const auto &constant_expr = dynamic_cast<const BoundConstant &>(expr);
      // 调用 PlanConstant 方法来处理常量表达式
      return std::make_tuple(UNNAMED_COLUMN, PlanConstant(constant_expr, children));
    }
    case ExpressionType::ALIAS: {
      // 将表达式转换为 BoundAlias 对象
      const auto &alias_expr = dynamic_cast<const BoundAlias &>(expr);
      // 递归调用自身来处理别名表达式的子表达式
      auto [_1, expr] = PlanExpression(*alias_expr.child_, children);
      // 返回一个包含别名和子表达式的元组
      return std::make_tuple(alias_expr.alias_, std::move(expr));
    }
    default:
      break;
  }
  // 表达式类型不在上述几种类型中，则抛出一个异常
  throw Exception(fmt::format("expression type {} not supported in planner yet", expr.type_));
}

}  // namespace bustub
