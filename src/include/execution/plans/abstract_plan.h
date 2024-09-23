//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// abstract_plan.h
//
// Identification: src/include/execution/plans/abstract_plan.h
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "catalog/schema.h"
#include "fmt/format.h"

namespace bustub {

// 定义了一个通用的 CloneWithChildren 实现，用于派生类
// 它创建一个当前类的副本，并设置新的子节点，然后返回这个副本的智能指针
#define BUSTUB_PLAN_NODE_CLONE_WITH_CHILDREN(cname)                                                          \
  auto CloneWithChildren(std::vector<AbstractPlanNodeRef> children) const->std::unique_ptr<AbstractPlanNode> \
      override {                                                                                             \
    auto plan_node = cname(*this);                                                                           \
    plan_node.children_ = children;                                                                          \
    return std::make_unique<cname>(std::move(plan_node));                                                    \
  }

/** PlanType represents the types of plans that we have in our system. */
// 系统中所有可能的计划节点类型
enum class PlanType {
  SeqScan,          // 顺序扫描，用于遍历表中的所有记录
  IndexScan,        // 索引扫描，使用索引来快速查找符合条件的记录
  Insert,           // 插入操作，用于向表中插入新记录
  Update,           // 更新操作，用于更新表中的现有记录
  Delete,           // 删除操作，用于从表中删除记录
  Aggregation,      // 聚合操作，用于计算聚合函数（如 SUM、AVG、COUNT 等）
  Limit,            // 限制操作，用于限制返回的记录数量
  NestedLoopJoin,   // 嵌套循环连接，一种连接操作，通过嵌套循环来匹配记录
  NestedIndexJoin,  // 嵌套索引连接，使用索引来加速嵌套循环连接
  HashJoin,         // 哈希连接，一种连接操作，通过哈希表来匹配记录
  Filter,           // 过滤操作，用于筛选符合条件的记录
  Values,           // 值操作，用于生成一组常量值
  Projection,       // 投影操作，用于选择特定的列
  Sort,             // 排序操作，用于对记录进行排序
  TopN,             // 前 N 个操作，用于获取排序后的前 N 个记录
  MockScan          // 模拟扫描，用于测试或模拟数据扫描操作
};

class AbstractPlanNode;
using AbstractPlanNodeRef = std::shared_ptr<const AbstractPlanNode>;

/**
 * AbstractPlanNode represents all the possible types of plan nodes in our system.
 * Plan nodes are modeled as trees, so each plan node can have a variable number of children.
 * Per the Volcano model, the plan node receives the tuples of its children.
 * The ordering of the children may matter.
 */
// 所有计划节点的基类。计划节点在系统中被建模为树结构，每个计划节点可以有可变数量的子节点
// 根据 Volcano 模型，计划节点接收其子节点的元组
class AbstractPlanNode {
 public:
  /**
   * Create a new AbstractPlanNode with the specified output schema and children.
   * @param output_schema the schema for the output of this plan node
   * @param children the children of this plan node
   */
  AbstractPlanNode(SchemaRef output_schema, std::vector<AbstractPlanNodeRef> children)
      : output_schema_(std::move(output_schema)), children_(std::move(children)) {}

  /** Virtual destructor. */
  virtual ~AbstractPlanNode() = default;

  /** @return the schema for the output of this plan node */
  // 返回该计划节点的输出模式
  auto OutputSchema() const -> const Schema & { return *output_schema_; }

  /** @return the child of this plan node at index child_idx */
  // 返回指定索引的子节点
  auto GetChildAt(uint32_t child_idx) const -> AbstractPlanNodeRef { return children_[child_idx]; }

  /** @return the children of this plan node */
  // 返回所有子节点
  auto GetChildren() const -> const std::vector<AbstractPlanNodeRef> & { return children_; }

  /** @return the type of this plan node */
  // 返回计划节点的类型，纯虚函数
  virtual auto GetType() const -> PlanType = 0;

  /** @return the string representation of the plan node and its children */
  // 返回计划节点及其子节点的字符串表示
  auto ToString(bool with_schema = true) const -> std::string {
    if (with_schema) {
      return fmt::format("{} | {}{}", PlanNodeToString(), output_schema_, ChildrenToString(2, with_schema));
    }
    return fmt::format("{}{}", PlanNodeToString(), ChildrenToString(2, with_schema));
  }

  /** @return the cloned plan node with new children */
  // 返回带有新子节点的克隆计划节点，纯虚函数
  virtual auto CloneWithChildren(std::vector<AbstractPlanNodeRef> children) const
      -> std::unique_ptr<AbstractPlanNode> = 0;

  /**
   * The schema for the output of this plan node. In the volcano model, every plan node will spit out tuples,
   * and this tells you what schema this plan node's tuples will have.
   */
  // 存储该计划节点的输出模式
  SchemaRef output_schema_;

  /** The children of this plan node. */
  std::vector<AbstractPlanNodeRef> children_;

 protected:
  /** @return the string representation of the plan node itself */
  // 返回计划节点本身的字符串表示，默认返回 <unknown>
  virtual auto PlanNodeToString() const -> std::string { return "<unknown>"; }

  /** @return the string representation of the plan node's children */
  auto ChildrenToString(int indent, bool with_schema = true) const -> std::string;

 private:
};

}  // namespace bustub

// 定义了如何格式化 AbstractPlanNode 及其智能指针的子类
template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<bustub::AbstractPlanNode, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const T &x, FormatCtx &ctx) const {
    return fmt::formatter<std::string>::format(x.ToString(), ctx);
  }
};

template <typename T>
struct fmt::formatter<std::unique_ptr<T>, std::enable_if_t<std::is_base_of<bustub::AbstractPlanNode, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const std::unique_ptr<T> &x, FormatCtx &ctx) const {
    return fmt::formatter<std::string>::format(x->ToString(), ctx);
  }
};
