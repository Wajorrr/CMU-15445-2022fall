//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// filter_executor.h
//
// Identification: src/include/execution/executors/filter_executor.h
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <vector>

#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * The FilterExecutor executor executes a filter.
 */
class FilterExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new FilterExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The filter plan to be executed
   * @param child_executor The child executor that feeds the filter
   */
  // 接收三个参数：执行上下文 exec_ctx、需要执行的过滤计划 plan 以及提供数据的子执行器 child_executor
  FilterExecutor(ExecutorContext *exec_ctx, const FilterPlanNode *plan,
                 std::unique_ptr<AbstractExecutor> &&child_executor);

  /** Initialize the filter */
  // 初始化过滤器
  void Init() override;

  /**
   * Yield the next tuple from the filter.
   * @param[out] tuple The next tuple produced by the filter
   * @param[out] rid The next tuple RID produced by the filter
   * @return `true` if a tuple was produced, `false` if there are no more tuples
   */
  // 用于从过滤器中获取下一个元组
  // 接收两个输出参数：tuple 和 rid，分别表示下一个元组和其对应的行标识符（RID）
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the filter plan */
  // 返回过滤计划的输出模式
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

 private:
  /** The filter plan node to be executed */
  // 一个指向 FilterPlanNode 的指针，表示需要执行的过滤计划节点
  const FilterPlanNode *plan_;

  /** The child executor from which tuples are obtained */
  // 提供数据的子执行器。过滤器从子执行器中获取元组，并根据过滤条件进行筛选
  std::unique_ptr<AbstractExecutor> child_executor_;
};
}  // namespace bustub
