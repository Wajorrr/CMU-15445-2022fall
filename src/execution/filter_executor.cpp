#include "execution/executors/filter_executor.h"
#include "common/exception.h"
#include "type/value_factory.h"

namespace bustub {

// 三个参数：执行上下文 exec_ctx、过滤计划节点 plan 和一个子执行器 child_executor
FilterExecutor::FilterExecutor(ExecutorContext *exec_ctx, const FilterPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void FilterExecutor::Init() {
  // Initialize the child executor
  // 初始化子执行器
  child_executor_->Init();
}

// 获取下一个满足过滤条件的元组
auto FilterExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 元组所需要满足的过滤条件
  auto filter_expr = plan_->GetPredicate();

  while (true) {
    // Get the next tuple
    // 在循环中调用子执行器的 Next 方法获取下一个元组
    const auto status = child_executor_->Next(tuple, rid);

    // 没有更多的元组可供处理
    if (!status) {
      return false;
    }

    // 对元组进行评估
    auto value = filter_expr->Evaluate(tuple, child_executor_->GetOutputSchema());
    // 如果获取到一个元组tuple，则使用过滤表达式对其进行评估
    // 如果评估结果value为非空且为 true，则返回 true
    if (!value.IsNull() && value.GetAs<bool>()) {
      return true;
    }
  }
}

}  // namespace bustub
