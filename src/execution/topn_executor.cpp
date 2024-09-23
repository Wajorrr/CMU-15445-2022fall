#include "execution/executors/topn_executor.h"

namespace bustub {

TopNExecutor::TopNExecutor(ExecutorContext *exec_ctx, const TopNPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void TopNExecutor::Init() {
  // 初始化子执行器
  child_executor_->Init();
  cur_index_ = 0;
  top_n_tuples_.clear();

  // 定义了一个排序比较函数 cmp，用于比较两个元组的顺序
  auto cmp = [this](const Tuple &a, const Tuple &b) -> bool {
    for (const auto &c : plan_->GetOrderBy()) {
      Value a_v = c.second->Evaluate(&a, child_executor_->GetOutputSchema());
      Value b_v = c.second->Evaluate(&b, child_executor_->GetOutputSchema());
      OrderByType order_by_type = c.first;
      if (a_v.CompareEquals(b_v) == CmpBool::CmpFalse) {
        assert(order_by_type != OrderByType::INVALID);
        if (order_by_type == OrderByType::DEFAULT || order_by_type == OrderByType::ASC) {
          return a_v.CompareGreaterThan(b_v) == CmpBool::CmpTrue;
        }
        return a_v.CompareLessThan(b_v) == CmpBool::CmpTrue;
      }
    }
    return false;
  };
  // 定义了一个优先队列 q，使用前面定义的比较函数 cmp 进行排序
  std::priority_queue<Tuple, std::vector<Tuple>, decltype(cmp)> q(cmp);
  Tuple child_tuple{};
  RID child_rid{};
  // 从子执行器中获取所有元组，并将这些元组存储在优先队列中
  while (child_executor_->Next(&child_tuple, &child_rid)) {
    q.emplace(child_tuple);
  }
  // 通过一个 for 循环，代码从优先队列中依次提取元组，直到提取到 N 个元组或优先队列为空为止
  for (size_t i = 0; i < plan_->GetN() && !q.empty(); ++i) {
    auto t = q.top();
    q.pop();
    top_n_tuples_.emplace_back(t);
  }
}

auto TopNExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (cur_index_ >= top_n_tuples_.size()) {
    return false;
  }
  *tuple = top_n_tuples_[cur_index_++];
  return true;
}

}  // namespace bustub
