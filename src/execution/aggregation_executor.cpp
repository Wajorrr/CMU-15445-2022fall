//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <vector>

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_(std::move(child)),
      aht_{SimpleAggregationHashTable{plan->GetAggregates(), plan->GetAggregateTypes()}},
      aht_iterator_(aht_.Begin()) {}

void AggregationExecutor::Init() {
  // 初始化子执行器
  child_->Init();
  // 清空聚合哈希表 aht_
  aht_.Clear();
  Tuple child_tuple{};
  RID child_rid{};
  while (child_->Next(&child_tuple, &child_rid)) {
    // 对于每个元组，代码调用 MakeAggregateKey 和 MakeAggregateValue 方法生成聚合键和值
    // 并将它们插入到聚合哈希表中
    aht_.InsertCombine(MakeAggregateKey(&child_tuple), MakeAggregateValue(&child_tuple));
  }
  // 检查聚合哈希表 aht_ 是否为空，如果为空且输出模式只有一列，则调用 InsertIntialCombine 方法
  // 判断输出模式的列数是否为 1 是为了处理某些特殊情况，例如 COUNT(*) 操作
  // 在这些情况下，即使没有输入元组，聚合操作仍然需要返回一个结果
  // SELECT COUNT(*) FROM table 在表为空时应该返回 0，而不是没有结果
  if (aht_.Begin() == aht_.End() && GetOutputSchema().GetColumnCount() == 1) {
    aht_.InsertIntialCombine();
  }
  // 将聚合哈希表的迭代器 aht_iterator_ 初始化为哈希表的起始位置
  aht_iterator_ = aht_.Begin();
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 检查聚合哈希表的迭代器 aht_iterator_ 是否已经到达哈希表的末尾
  if (aht_iterator_ == aht_.End()) {
    return false;
  }
  // 如果迭代器没有到达末尾，代码会构建一个包含元组值的向量 values
  std::vector<Value> values;
  // 将当前迭代器键中的分组字段（group_bys_）插入到 values 向量的末尾
  values.insert(values.end(), aht_iterator_.Key().group_bys_.begin(), aht_iterator_.Key().group_bys_.end());
  // 将当前迭代器值中的聚合结果（aggregates_）插入到 values 向量的末尾
  values.insert(values.end(), aht_iterator_.Val().aggregates_.begin(), aht_iterator_.Val().aggregates_.end());
  *tuple = Tuple{values, &GetOutputSchema()};
  ++aht_iterator_;
  return true;
}

auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_.get(); }

}  // namespace bustub
