#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "binder/table_ref/bound_subquery_ref.h"
#include "binder/tokens.h"
#include "catalog/catalog.h"
#include "catalog/column.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/plans/aggregation_plan.h"

namespace bustub {

class BoundStatement;
class SelectStatement;
class DeleteStatement;
class AbstractPlanNode;
class InsertStatement;
class BoundExpression;
class BoundTableRef;
class BoundBinaryOp;
class BoundConstant;
class BoundColumnRef;
class BoundUnaryOp;
class BoundBaseTableRef;
class BoundSubqueryRef;
class BoundCrossProductRef;
class BoundJoinRef;
class BoundExpressionListRef;
class BoundAggCall;
class BoundCTERef;
class ColumnValueExpression;

/**
 * The context for the planner. Used for planning aggregation calls.
 */
// PlannerContext 类用于在查询规划过程中管理聚合调用的上下文
class PlannerContext {
 public:
  PlannerContext() = default;

  // 用于添加聚合表达式
  void AddAggregation(std::unique_ptr<BoundExpression> expr);

  /** Indicates whether aggregation is allowed in this context. */
  bool allow_aggregation_{false};  // 是否允许在当前上下文中进行聚合

  /** Indicates the next agg call to be processed in this context. */
  size_t next_aggregation_{0};  // 下一个要处理的聚合调用

  /**
   * In the first phase of aggregation planning, we put all agg calls expressions into this vector.
   * The expressions in this vector should be used over the output of the original filter / table
   * scan plan node.
   */
  std::vector<std::unique_ptr<BoundExpression>> aggregations_;  // 存储所有聚合调用表达式的向量

  /**
   * In the second phase of aggregation planning, we plan agg calls from `aggregations_`, and generate
   * an aggregation plan node. The expressions in thie vector should be used over the output from the
   * aggregation plan node.
   */
  std::vector<AbstractExpressionRef> expr_in_agg_;  // 存储在聚合计划节点输出中使用的表达式的向量

  /**
   * CTE in scope.
   */
  const CTEList *cte_list_{nullptr};  // 当前作用域中的公共表表达式（CTE）
};

/**
 * The planner takes a bound statement, and transforms it into the BusTub plan tree.
 * The plan tree will be taken by the execution engine to execute the statement.
 */
// 负责将绑定的语句转换为 BusTub 的计划树，计划树将由执行引擎执行
class Planner {
 public:
  explicit Planner(const Catalog &catalog) : catalog_(catalog) {}

  // The following parts are undocumented. One `PlanXXX` functions simply corresponds to a
  // bound thing in the binder.

  void PlanQuery(const BoundStatement &statement);

  auto PlanSelect(const SelectStatement &statement) -> AbstractPlanNodeRef;

  /**
   * @brief Plan a `BoundTableRef`
   *
   * - For a BaseTableRef, this function will return a `SeqScanPlanNode`. Note that all tables with
   *   names beginning with `__` will be planned as `MockScanPlanNode`.
   * - For a `JoinRef` or `CrossProductRef`, this function will return a `NestedLoopJoinNode`.
   * @param table_ref the bound table ref from binder.
   * @return the plan node of this bound table ref.
   */
  auto PlanTableRef(const BoundTableRef &table_ref) -> AbstractPlanNodeRef;

  auto PlanSubquery(const BoundSubqueryRef &table_ref, const std::string &alias) -> AbstractPlanNodeRef;

  auto PlanBaseTableRef(const BoundBaseTableRef &table_ref) -> AbstractPlanNodeRef;

  auto PlanCrossProductRef(const BoundCrossProductRef &table_ref) -> AbstractPlanNodeRef;

  auto PlanJoinRef(const BoundJoinRef &table_ref) -> AbstractPlanNodeRef;

  auto PlanCTERef(const BoundCTERef &table_ref) -> AbstractPlanNodeRef;

  auto PlanExpressionListRef(const BoundExpressionListRef &table_ref) -> AbstractPlanNodeRef;

  void AddAggCallToContext(BoundExpression &expr);

  auto PlanExpression(const BoundExpression &expr, const std::vector<AbstractPlanNodeRef> &children)
      -> std::tuple<std::string, AbstractExpressionRef>;

  auto PlanBinaryOp(const BoundBinaryOp &expr, const std::vector<AbstractPlanNodeRef> &children)
      -> AbstractExpressionRef;

  auto PlanColumnRef(const BoundColumnRef &expr, const std::vector<AbstractPlanNodeRef> &children)
      -> std::tuple<std::string, std::shared_ptr<ColumnValueExpression>>;

  auto PlanConstant(const BoundConstant &expr, const std::vector<AbstractPlanNodeRef> &children)
      -> AbstractExpressionRef;

  auto PlanSelectAgg(const SelectStatement &statement, AbstractPlanNodeRef child) -> AbstractPlanNodeRef;

  auto PlanAggCall(const BoundAggCall &agg_call, const std::vector<AbstractPlanNodeRef> &children)
      -> std::tuple<AggregationType, std::vector<AbstractExpressionRef>>;

  auto GetAggCallFromFactory(const std::string &func_name, std::vector<AbstractExpressionRef> args)
      -> std::tuple<AggregationType, std::vector<AbstractExpressionRef>>;

  auto GetBinaryExpressionFromFactory(const std::string &op_name, AbstractExpressionRef left,
                                      AbstractExpressionRef right) -> AbstractExpressionRef;

  auto PlanInsert(const InsertStatement &statement) -> AbstractPlanNodeRef;

  auto PlanDelete(const DeleteStatement &statement) -> AbstractPlanNodeRef;

  auto PlanUpdate(const UpdateStatement &statement) -> AbstractPlanNodeRef;

  /** the root plan node of the plan tree */
  AbstractPlanNodeRef plan_;

 private:
  PlannerContext ctx_;

  // 嵌套类 ContextGuard，用于在修改上下文时保存和恢复 PlannerContext
  class ContextGuard {
   public:
    explicit ContextGuard(PlannerContext *ctx) : old_ctx_(std::move(*ctx)), ctx_ptr_(ctx) {
      *ctx = PlannerContext();
      ctx->cte_list_ = old_ctx_.cte_list_;
    }
    ~ContextGuard() { *ctx_ptr_ = std::move(old_ctx_); }

    DISALLOW_COPY_AND_MOVE(ContextGuard);

   private:
    PlannerContext old_ctx_;
    PlannerContext *ctx_ptr_;
  };

  /** If any function needs to modify the scope, it MUST hold the context guard, so that
   * the context will be recovered after the function returns. Currently, it's used in
   * `BindFrom` and `BindJoin`.
   */
  // NewContext 方法返回一个 ContextGuard 对象，以确保在函数返回后上下文能够恢复
  auto NewContext() -> ContextGuard { return ContextGuard(&ctx_); }

  // 用于生成输出模式
  auto MakeOutputSchema(const std::vector<std::pair<std::string, TypeId>> &exprs) -> SchemaRef;

  /** Catalog will be used during the planning process. SHOULD ONLY BE USED IN
   * CODE PATH OF `PlanQuery`, otherwise it's a dangling reference.
   */
  const Catalog &catalog_;  // 用于在规划过程中访问目录

  /** An id for all unnamed things */
  size_t universal_id_{0};  // 用于所有未命名对象的 ID
};

static constexpr const char *const UNNAMED_COLUMN = "<unnamed>";

}  // namespace bustub
