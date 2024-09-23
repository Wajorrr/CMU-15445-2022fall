//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// values_plan.h
//
// Identification: src/include/execution/plans/values_plan.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "catalog/catalog.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/plans/abstract_plan.h"

namespace bustub {

/**
 * The ValuesPlanNode represents rows of values. For example,
 * `INSERT INTO table VALUES ((0, 1), (1, 2))`, where we will have
 * `(0, 1)` and `(1, 2)` as the output of this executor.
 */
// ValuesPlanNode 是 BusTub 数据库系统中的一个执行计划节点，用于表示包含行值的计划节点
// 它继承自 AbstractPlanNode，并提供了特定于 ValuesPlanNode 的功能和属性
class ValuesPlanNode : public AbstractPlanNode {
 public:
  /**
   * Construct a new ValuesPlanNode instance.
   * @param output The output schema of this values plan node
   * @param values The values produced by this plan node
   */
  explicit ValuesPlanNode(SchemaRef output, std::vector<std::vector<AbstractExpressionRef>> values)
      : AbstractPlanNode(std::move(output), {}), values_(std::move(values)) {}

  /** @return The type of the plan node */
  // 返回计划节点的类型，这里是 PlanType::Values
  auto GetType() const -> PlanType override { return PlanType::Values; }

  // 返回由该计划节点生成的值
  auto GetValues() const -> const std::vector<std::vector<AbstractExpressionRef>> & { return values_; }

  // 使用宏定义简化克隆过程
  BUSTUB_PLAN_NODE_CLONE_WITH_CHILDREN(ValuesPlanNode);

  // 存储该计划节点生成的值
  std::vector<std::vector<AbstractExpressionRef>> values_;

 protected:
  // 返回计划节点的字符串表示，显示值的行数
  auto PlanNodeToString() const -> std::string override { return fmt::format("Values {{ rows={} }}", values_.size()); }
};

}  // namespace bustub
