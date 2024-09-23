#include "execution/executors/values_executor.h"

namespace bustub {

ValuesExecutor::ValuesExecutor(ExecutorContext *exec_ctx, const ValuesPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan), dummy_schema_(Schema({})) {}

void ValuesExecutor::Init() { cursor_ = 0; }

auto ValuesExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 检查 cursor_ 是否已经超过了计划节点中的值数量
  if (cursor_ >= plan_->GetValues().size()) {
    return false;
  }

  // 创建一个 values 向量，并预留空间以容纳输出模式中的列数
  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());

  // 获取当前行的表达式 row_expr，并遍历每一列的表达式
  // 使用 Evaluate 方法评估表达式，并将结果添加到 values 向量中
  const auto &row_expr = plan_->GetValues()[cursor_];
  for (const auto &col : row_expr) {
    values.push_back(col->Evaluate(nullptr, dummy_schema_));
  }
  // 使用 values 和输出模式创建一个新的 Tuple，将其赋值给 tuple
  *tuple = Tuple{values, &GetOutputSchema()};
  // 将 cursor_ 增加 1，表示已经处理了一行
  cursor_ += 1;

  // 返回 true，表示成功获取了一个元组
  return true;
}

}  // namespace bustub
