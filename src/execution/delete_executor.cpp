//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "common/exception.h"
#include "execution/executors/delete_executor.h"

namespace bustub {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {
  // 调用子执行器的 Init 方法进行初始化
  child_executor_->Init();
  // 将 is_end_ 标志设置为 false，表示删除操作尚未结束
  is_end_ = false;
  // 从执行上下文中获取锁管理器和当前事务对象
  auto lock_manager = exec_ctx_->GetLockManager();
  auto txn = exec_ctx_->GetTransaction();
  // 尝试获取表的意向排他锁（INTENTION_EXCLUSIVE）以确保在删除数据时不会发生并发读写操作
  try {
    auto status = lock_manager->LockTable(txn, LockManager::LockMode::INTENTION_EXCLUSIVE, plan_->TableOid());
    if (!status) {
      throw ExecutionException{"Delete Executor Get Table Lock Failed"};
    }
  } catch (TransactionAbortException &e) {
    throw ExecutionException{"Delete Executor Get Table Lock Failed" + e.GetInfo()};
  }
}

auto DeleteExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  // 检查 is_end_ 标志是否为 true，以确定删除操作是否已经结束
  if (is_end_) {
    return false;
  }
  // 从执行上下文中获取表的信息和与该表相关的所有索引信息
  const TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->TableOid());
  std::vector<IndexInfo *> indexs = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);
  // 初始化一个空的 Tuple 对象 child_tuple，用于存储子执行器返回的元组
  Tuple child_tuple{};
  // 记录删除的元组数量
  int32_t delete_count{0};
  // 获取锁管理器和当前事务对象
  auto lock_manager = exec_ctx_->GetLockManager();
  auto txn = exec_ctx_->GetTransaction();

  // 调用子执行器的 Next 方法获取下一个元组，并获取该元组的行标识符 child_rid
  while (child_executor_->Next(&child_tuple, rid)) {
    RID child_rid = child_tuple.GetRid();
    try {
      // 尝试获取该行的排他锁
      auto status = lock_manager->LockRow(txn, LockManager::LockMode::EXCLUSIVE, plan_->TableOid(), child_rid);
      if (!status) {
        throw ExecutionException{"Delete Executor Get Row Lock Failed"};
      }
    } catch (TransactionAbortException &e) {
      throw ExecutionException{"Delete Executor Get Row Lock Failed" + e.GetInfo()};
    }
    // 代码调用表的 MarkDelete 方法标记该行已删除
    table_info->table_->MarkDelete(child_rid, exec_ctx_->GetTransaction());
    // 遍历所有与表相关的索引，并将删除的元组从每个索引中删除
    for (auto index : indexs) {
      // 一定要注意是怎么删除的
      // 调用索引的 DeleteEntry 方法，传入从元组中提取的键、行的 RID 和当前事务
      index->index_->DeleteEntry(
          child_tuple.KeyFromTuple(table_info->schema_, index->key_schema_, index->index_->GetKeyAttrs()), child_rid,
          exec_ctx_->GetTransaction());
      const Tuple tmp = child_tuple;
      // 将删除操作记录到事务的索引写集合中，以便在事务回滚时能够撤销这些操作
      txn->GetIndexWriteSet()->push_back(IndexWriteRecord(child_rid, plan_->TableOid(), WType::DELETE, tmp,
                                                          index->index_oid_, exec_ctx_->GetCatalog()));
    }
    delete_count++;
  }
  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());
  values.emplace_back(TypeId::INTEGER, delete_count);
  // 返回的Tuple是自定义的，返回的是删除的行数
  *tuple = Tuple{values, &GetOutputSchema()};
  // 将 is_end_ 标志设置为 true，表示删除操作已经结束
  is_end_ = true;
  return true;
}

}  // namespace bustub
