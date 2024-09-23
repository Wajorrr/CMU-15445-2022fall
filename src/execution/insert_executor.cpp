//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  // 初始化子执行器
  child_executor_->Init();
  // 将 is_end_ 标志设置为 false，表示插入操作尚未结束
  is_end_ = false;
  // 从执行上下文中获取锁管理器和当前事务对象
  auto lock_manager = exec_ctx_->GetLockManager();
  auto txn = exec_ctx_->GetTransaction();
  // 尝试获取表的意向排他锁（INTENTION_EXCLUSIVE）以确保在插入数据时不会发生并发读写操作
  try {
    auto status = lock_manager->LockTable(txn, LockManager::LockMode::INTENTION_EXCLUSIVE, plan_->TableOid());
    if (!status) {
      throw ExecutionException{"Insert Executor Get Table Lock Failed"};
    }
  } catch (TransactionAbortException &e) {
    throw ExecutionException{"Insert Executor Get Table Lock Failed" + e.GetInfo()};
  }
}

auto InsertExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  // 检查插入操作是否已经结束
  if (is_end_) {
    return false;
  }
  // 初始化插入计数 insert_count，并从执行上下文中获取表信息、索引信息、锁管理器和当前事务对象
  int32_t insert_count = 0;
  const TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->TableOid());
  std::vector<IndexInfo *> indexs = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);
  auto lock_manager = exec_ctx_->GetLockManager();
  auto txn = exec_ctx_->GetTransaction();

  // 从子执行器中获取下一个元组，然后将其插入到表中
  Tuple child_tuple{};
  while (child_executor_->Next(&child_tuple, rid)) {
    table_info->table_->InsertTuple(child_tuple, rid, exec_ctx_->GetTransaction());
    // insert应该是要插入后再在行上加锁
    // 插入元组后，代码尝试获取该行的排他锁（LockManager::LockMode::EXCLUSIVE）
    // 以确保在插入数据时不会发生并发写操作
    try {
      auto status = lock_manager->LockRow(txn, LockManager::LockMode::EXCLUSIVE, plan_->TableOid(), *(rid));
      if (!status) {
        throw ExecutionException{"Insert Executor Get Row Lock Failed"};
      }
    } catch (TransactionAbortException &e) {
      throw ExecutionException{"Insert Executor Get Row Lock Failed" + e.GetInfo()};
    }

    // 遍历所有与表相关的索引，并将插入的元组更新到每个索引中
    for (auto index : indexs) {
      // 调用索引的 InsertEntry 方法，传入从元组中提取的键、行的 RID 和当前事务
      // 插入的<key, value> = <key, rid>
      index->index_->InsertEntry(
          child_tuple.KeyFromTuple(table_info->schema_, index->key_schema_, index->index_->GetKeyAttrs()), *rid,
          exec_ctx_->GetTransaction());
      const Tuple tmp = child_tuple;
      // 将插入操作记录到事务的索引写集合中，以便在事务回滚时能够撤销这些操作
      txn->GetIndexWriteSet()->push_back(
          IndexWriteRecord(*rid, plan_->TableOid(), WType::INSERT, tmp, index->index_oid_, exec_ctx_->GetCatalog()));
    }
    insert_count++;
  }
  // 创建一个 values 向量，并将插入计数作为一个 Value 对象添加到向量中
  std::vector<Value> values{};
  values.reserve(GetOutputSchema().GetColumnCount());
  values.emplace_back(TypeId::INTEGER, insert_count);
  // 使用这些值创建一个新的 Tuple 对象，并将其赋值给输出参数 tuple
  // 输出的Tuple形式是自定义的，返回的是插入的行数
  *tuple = Tuple{values, &GetOutputSchema()};
  // 将 is_end_ 标志设置为 true，表示插入操作已经结束
  is_end_ = true;
  return true;
}

}  // namespace bustub
