#include "execution/executors/projection_executor.h"
#include "storage/table/tuple.h"

namespace bustub {

ProjectionExecutor::ProjectionExecutor(ExecutorContext *exec_ctx, const ProjectionPlanNode *plan,
                                       std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void ProjectionExecutor::Init() {
  // Initialize the child executor
  child_executor_->Init();
}

auto ProjectionExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 创建了一个空的 Tuple 对象 child_tuple
  // 然后，它调用子执行器的 Next 方法来获取下一个元组，并将结果存储在 child_tuple 中
  Tuple child_tuple{};

  // Get the next tuple
  const auto status = child_executor_->Next(&child_tuple, rid);

  if (!status) {
    return false;
  }

  // Compute expressions
  // 创建一个 values 向量，并预留空间以容纳输出模式中的列数
  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());
  // 遍历计划节点中的每个表达式，使用 Evaluate 方法评估每个表达式，并将结果添加到 values 向量中
  for (const auto &expr : plan_->GetExpressions()) {
    values.push_back(expr->Evaluate(&child_tuple, child_executor_->GetOutputSchema()));
  }

  // 使用计算得到的 values 和输出模式创建一个新的 Tuple，并将其赋值给传入的 tuple 指针
  *tuple = Tuple{values, &GetOutputSchema()};

  return true;
}
}  // namespace bustub
