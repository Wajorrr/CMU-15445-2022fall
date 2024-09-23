#include "execution/executors/sort_executor.h"

namespace bustub {

SortExecutor::SortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      order_by_(plan->GetOrderBy()) {}

void SortExecutor::Init() {
  // 初始化排序执行器并准备排序操作
  child_executor_->Init();
  Tuple child_tuple{};
  RID child_rid{};
  tuples_.clear();
  // 从子执行器中获取所有元组，并将这些元组存储在内部的 tuples_ 向量中
  while (child_executor_->Next(&child_tuple, &child_rid)) {
    tuples_.emplace_back(child_tuple);
  }
  // 定义了一个排序比较函数 cmp，用于比较两个元组的顺序
  // 该函数遍历排序条件 order_by_，依次比较每个条件下两个元组的值
  // 如果两个元组在某个条件下的值不相等，则根据排序类型（升序或降序）决定它们的顺序
  auto cmp = [this](const Tuple &a, const Tuple &b) -> bool {
    for (const auto &c : order_by_) {
      // 遍历排序条件 order_by_，每个排序条件包含一个排序类型和一个用于评估元组值的表达式
      // 使用表达式评估两个元组在该条件下的值
      Value a_v = c.second->Evaluate(&a, child_executor_->GetOutputSchema());
      Value b_v = c.second->Evaluate(&b, child_executor_->GetOutputSchema());
      // 获取当前排序条件的排序类型 order_by_type，并比较两个元组的值
      OrderByType order_by_type = c.first;
      // 如果两个元组的值不相等，代码会根据排序类型决定它们的顺序
      if (a_v.CompareEquals(b_v) == CmpBool::CmpFalse) {
        assert(order_by_type != OrderByType::INVALID);
        if (order_by_type == OrderByType::DEFAULT || order_by_type == OrderByType::ASC) {
          return a_v.CompareLessThan(b_v) == CmpBool::CmpTrue;
        }
        return a_v.CompareGreaterThan(b_v) == CmpBool::CmpTrue;
      }
    }
    return false;
  };
  // 对元组进行排序，使用前面定义的比较函数 cmp
  std::sort(tuples_.begin(), tuples_.end(), cmp);
  index_ = 0;
}

auto SortExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (index_ >= tuples_.size()) {
    return false;
  }
  *tuple = tuples_[index_++];
  return true;
}

}  // namespace bustub
