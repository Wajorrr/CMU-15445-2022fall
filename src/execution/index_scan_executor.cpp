//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/index_scan_executor.h"

namespace bustub {
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx),
      plan_{plan},
      index_info_{exec_ctx->GetCatalog()->GetIndex(plan->GetIndexOid())},
      tree_{dynamic_cast<BPlusTreeIndexForOneIntegerColumn *>(index_info_->index_.get())},
      iter_{tree_->GetBeginIterator()} {}

void IndexScanExecutor::Init() {}

auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // 检查迭代器 iter_ 是否已经到达 B+ 树的末尾
  if (iter_ == tree_->GetEndIterator()) {
    // 如果迭代器到达末尾，表示没有更多的元组可以扫描
    tuple = nullptr;
    rid = nullptr;
    return false;
  }
  // 如果迭代器没有到达末尾，代码获取当前迭代器指向的元组的 RID（行标识符）cur_rid
  // 然后将迭代器移动到下一个位置
  // <key, value> = <key, rid>
  RID cur_rid = (*iter_).second;
  ++iter_;
  // 从执行上下文中获取表的信息，并调用表的 GetTuple 方法，使用 cur_rid 检索元组数据
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(index_info_->table_name_);
  auto status = table_info->table_->GetTuple(cur_rid, tuple, exec_ctx_->GetTransaction());
  if (!status) {
    LOG_DEBUG("error");
  }
  // 将 cur_rid 赋值给输出参数 rid
  *rid = cur_rid;
  return true;
}

}  // namespace bustub
