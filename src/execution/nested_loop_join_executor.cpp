//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"
#include "type/value_factory.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2022 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

// 初始化左右两个执行器，并将它们的输出存储在内部表中，以便后续的嵌套循环连接操作使用
void NestedLoopJoinExecutor::Init() {
  // 初始化左右两个执行器
  left_executor_->Init();
  right_executor_->Init();
  Tuple tuple{};
  RID rid{};
  // 从左执行器中读取元组，并将这些元组存储在内部的 left_table_ 向量中
  while (left_executor_->Next(&tuple, &rid)) {
    left_table_.emplace_back(tuple);
  }
  // 从右执行器中读取元组，并将这些元组存储在内部的 right_table_ 向量中
  while (right_executor_->Next(&tuple, &rid)) {
    right_table_.emplace_back(tuple);
  }
  // 初始化左右两个表的索引
  left_index_ = 0;
  right_index_ = 0;
  is_none_ = true;
}

// 遍历左右表中的元组，并根据连接条件生成符合条件的连接结果
auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 获取连接条件表达式 expr，用于后续的连接条件判断
  auto expr = &plan_->Predicate();

  // 遍历左右表中的元组并生成连接结果
  while (true) {
    // 检查左表是否已经遍历完
    if (left_index_ == left_table_.size()) {
      return false;
    }
    // 检查右表是否不为空
    if (!right_table_.empty()) {
      // 调用连接条件表达式的 EvaluateJoin 方法，判断当前左表元组和右表元组是否满足连接条件
      auto value = expr->EvaluateJoin(&left_table_[left_index_], left_executor_->GetOutputSchema(),
                                      &right_table_[right_index_], right_executor_->GetOutputSchema());
      if (!value.IsNull() && value.GetAs<bool>()) {
        // 如果连接条件满足，生成一个新的连接结果元组
        // 将 is_none_ 标志设置为 false，表示当前左表元组有匹配的右表元组
        is_none_ = false;
        std::vector<Value> values{};
        // 将左表和右表的所有列值合并到一个新的向量中，并使用该向量生成一个新的元组
        for (uint32_t idx = 0; idx < left_executor_->GetOutputSchema().GetColumnCount(); idx++) {
          values.push_back(left_table_[left_index_].GetValue(&left_executor_->GetOutputSchema(), idx));
        }
        for (uint32_t idx = 0; idx < right_executor_->GetOutputSchema().GetColumnCount(); idx++) {
          values.push_back(right_table_[right_index_].GetValue(&right_executor_->GetOutputSchema(), idx));
        }
        *tuple = Tuple{values, &GetOutputSchema()};
        // 更新右表索引，如果右表已经遍历完，则重置右表索引并更新左表索引
        if (right_index_ < right_table_.size() - 1) {
          right_index_++;
          assert(right_index_ < right_table_.size());
        } else {
          // 重置
          is_none_ = true;
          right_index_ = 0;
          left_index_++;
        }
        return true;
      }
    }
    // 如果右表还没有遍历完，更新右表索引，继续处理下一个右表元组
    if (!right_table_.empty() && right_index_ < right_table_.size() - 1) {
      right_index_++;
    } else {
      // 如果右表已经遍历完，检查当前左表元组是否没有匹配的右表元组，并且连接类型是否为左连接
      if (is_none_ && plan_->GetJoinType() == JoinType::LEFT) {
        std::vector<Value> values{};
        // 如果是左连接，代码会生成一个包含左表元组和右表 NULL 值的连接结果元组
        for (uint32_t idx = 0; idx < left_executor_->GetOutputSchema().GetColumnCount(); idx++) {
          values.push_back(left_table_[left_index_].GetValue(&left_executor_->GetOutputSchema(), idx));
        }
        for (uint32_t idx = 0; idx < right_executor_->GetOutputSchema().GetColumnCount(); idx++) {
          values.push_back(
              ValueFactory::GetNullValueByType(right_executor_->GetOutputSchema().GetColumn(idx).GetType()));
        }
        *tuple = Tuple{values, &GetOutputSchema()};
        // 重置右表索引并更新左表索引，继续处理下一个左表元组
        right_index_ = 0;
        left_index_++;
        is_none_ = true;
        return true;
      }
      // 如果右表已经遍历完，重置右表索引并更新左表索引，继续处理下一个左表元组
      right_index_ = 0;
      left_index_++;
      is_none_ = true;
    }
  }
}

}  // namespace bustub
