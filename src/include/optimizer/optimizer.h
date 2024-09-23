#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "catalog/catalog.h"
#include "concurrency/transaction.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/plans/abstract_plan.h"

#define BUSTUB_OPTIMIZER_HACK_REMOVE_AFTER_2022_FALL

namespace bustub {

/**
 * The optimizer takes an `AbstractPlanNode` and outputs an optimized `AbstractPlanNode`.
 */
// 对查询计划进行优化
class Optimizer {
 public:
  explicit Optimizer(const Catalog &catalog, bool force_starter_rule)
      : catalog_(catalog), force_starter_rule_(force_starter_rule) {}
  // 接收一个 AbstractPlanNode 对象，并输出一个优化后的 AbstractPlanNode 对象
  // 用于执行标准优化
  auto Optimize(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;
  // 用于执行自定义优化
  auto OptimizeCustom(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

 private:
  /**
   * @brief merge projections that do identical project.
   * Identical projection might be produced when there's `SELECT *`, aggregation, or when we need to rename the columns
   * in the planner. We merge these projections so as to make execution faster.
   */
  // 合并相同的投影操作，以提高执行效率
  auto OptimizeMergeProjection(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief merge filter condition into nested loop join.
   * In planner, we plan cross join + filter with cross product (done with nested loop join) and a filter plan node. We
   * can merge the filter condition into nested loop join to achieve better efficiency.
   */
  // 将过滤条件合并到嵌套循环连接中，以提高效率
  auto OptimizeMergeFilterNLJ(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief optimize nested loop join into hash join.
   * In the starter code, we will check NLJs with exactly one equal condition. You can further support optimizing joins
   * with multiple eq conditions.
   */
  // 将嵌套循环连接优化为哈希连接
  auto OptimizeNLJAsHashJoin(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief optimize nested loop join into index join.
   */
  // 将嵌套循环连接优化为索引连接
  auto OptimizeNLJAsIndexJoin(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief eliminate always true filter
   */
  // 消除总是为真的过滤条件
  auto OptimizeEliminateTrueFilter(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief merge filter into filter_predicate of seq scan plan node
   */
  // 将过滤条件合并到顺序扫描计划节点的过滤谓词中
  auto OptimizeMergeFilterScan(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief rewrite expression to be used in nested loop joins. e.g., if we have `SELECT * FROM a, b WHERE a.x = b.y`,
   * we will have `#0.x = #0.y` in the filter plan node. We will need to figure out where does `0.x` and `0.y` belong
   * in NLJ (left table or right table?), and rewrite it as `#0.x = #1.y`.
   *
   * @param expr the filter expression
   * @param left_column_cnt number of columns in the left size of the NLJ
   * @param right_column_cnt number of columns in the left size of the NLJ
   */
  // 重写用于嵌套循环连接的表达式
  auto RewriteExpressionForJoin(const AbstractExpressionRef &expr, size_t left_column_cnt, size_t right_column_cnt)
      -> AbstractExpressionRef;

  /** @brief check if the predicate is true::boolean */
  // 检查谓词是否总是为真
  auto IsPredicateTrue(const AbstractExpression &expr) -> bool;

  /**
   * @brief optimize order by as index scan if there's an index on a table
   */
  // 如果表上有索引，将排序操作优化为索引扫描
  auto OptimizeOrderByAsIndexScan(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /** @brief check if the index can be matched */
  // 检查索引是否匹配
  auto MatchIndex(const std::string &table_name, uint32_t index_key_idx)
      -> std::optional<std::tuple<index_oid_t, std::string>>;

  /**
   * @brief optimize sort + limit as top N
   */
  // 将排序加限制操作优化为 Top N 操作
  auto OptimizeSortLimitAsTopN(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef;

  /**
   * @brief get the estimated cardinality for a table based on the table name. Useful when join reordering. BusTub
   * doesn't support statistics for now, so it's the only way for you to get the table size :(
   *
   * @param table_name
   * @return std::optional<size_t>
   */
  // 基于表名获取表的估计基数，用于连接重排序
  auto EstimatedCardinality(const std::string &table_name) -> std::optional<size_t>;

  /** Catalog will be used during the planning process. USERS SHOULD ENSURE IT OUTLIVES
   * OPTIMIZER, otherwise it's a dangling reference.
   */
  const Catalog &catalog_;

  // 用于控制是否强制应用初始规则
  const bool force_starter_rule_;
};

}  // namespace bustub
