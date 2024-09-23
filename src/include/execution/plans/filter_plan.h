//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// filter_plan.h
//
// Identification: src/include/execution/plans/filter_plan.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "catalog/catalog.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/plans/abstract_plan.h"

namespace bustub {

/**
 * The FilterPlanNode represents a filter operation.
 * It retains any tuple that satisfies the predicate in the child.
 */
// 保留满足子节点中谓词条件的元组
class FilterPlanNode : public AbstractPlanNode {
 public:
  /**
   * Construct a new FilterPlanNode instance.
   * @param output The output schema of this filter plan node
   * @param predicate The predicate applied during the scan operation
   * @param child The child plan node
   */
  // 三个参数：输出模式 output、谓词 predicate 和子计划节点 child
  FilterPlanNode(SchemaRef output, AbstractExpressionRef predicate, AbstractPlanNodeRef child)
      : AbstractPlanNode(std::move(output), {std::move(child)}), predicate_{std::move(predicate)} {}

  /** @return The type of the plan node */
  // 返回计划节点的类型
  auto GetType() const -> PlanType override { return PlanType::Filter; }

  /** @return The predicate to test tuples against; tuples should only be returned if they evaluate to true */
  // 返回谓词表达式的引用，用于测试元组是否满足条件
  auto GetPredicate() const -> const AbstractExpressionRef & { return predicate_; }

  /** @return The child plan node */
  // 返回子计划节点，并断言子计划节点的数量必须为一个
  auto GetChildPlan() const -> AbstractPlanNodeRef {
    BUSTUB_ASSERT(GetChildren().size() == 1, "Filter should have exactly one child plan.");
    return GetChildAt(0);
  }

  BUSTUB_PLAN_NODE_CLONE_WITH_CHILDREN(FilterPlanNode);

  /** The predicate that all returned tuples must satisfy */
  // 成员变量 predicate_ 存储了所有返回元组必须满足的谓词条件
  // 调用 Evaluate 方法来评估谓词表达式
  // 调用 EvaluateJoin 方法来评估连接操作中的谓词表达式
  AbstractExpressionRef predicate_;

 protected:
  auto PlanNodeToString() const -> std::string override {
    return fmt::format("Filter {{ predicate={} }}", *predicate_);
  }
};

}  // namespace bustub
