//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// seq_scan_executor.cpp
//
// Identification: src/execution/seq_scan_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/seq_scan_executor.h"

namespace bustub {

SeqScanExecutor::SeqScanExecutor(ExecutorContext *exec_ctx, const SeqScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {
  this->table_info_ = this->exec_ctx_->GetCatalog()->GetTable(plan_->table_oid_);
}

void SeqScanExecutor::Init() {
  // 检查当前事务的隔离级别是否为 READ_UNCOMMITTED。如果不是，则需要获取表的意向共享锁
  if (exec_ctx_->GetTransaction()->GetIsolationLevel() != IsolationLevel::READ_UNCOMMITTED) {
    // 尝试获取表的意向共享锁（INTENTION_SHARED）以确保在读取数据时不会发生并发写操作
    try {
      bool is_locked = exec_ctx_->GetLockManager()->LockTable(
          exec_ctx_->GetTransaction(), LockManager::LockMode::INTENTION_SHARED, table_info_->oid_);
      if (!is_locked) {
        // throw ExecutionException("SeqScan Executor Get Table Lock Failed");
      }
    } catch (TransactionAbortException &e) {
      // throw ExecutionException("SeqScan Executor Get Table Lock Failed" + e.GetInfo());
    }
  }
  // 初始化表迭代器 table_iter_
  // 使其指向表的开始位置，以便后续的顺序扫描操作能够从表的起始位置开始读取数据
  this->table_iter_ = table_info_->table_->Begin(exec_ctx_->GetTransaction());
}

auto SeqScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  do {
    // 检查表迭代器是否已经到达表的末尾
    if (table_iter_ == table_info_->table_->End()) {
      if (exec_ctx_->GetTransaction()->GetIsolationLevel() == IsolationLevel::READ_COMMITTED) {
        // 释放所有持有的行锁和表锁
        const auto locked_row_set = exec_ctx_->GetTransaction()->GetSharedRowLockSet()->at(table_info_->oid_);
        table_oid_t oid = table_info_->oid_;
        for (auto rid : locked_row_set) {
          exec_ctx_->GetLockManager()->UnlockRow(exec_ctx_->GetTransaction(), oid, rid);
        }

        exec_ctx_->GetLockManager()->UnlockTable(exec_ctx_->GetTransaction(), table_info_->oid_);
      }
      // 返回 false，表示没有更多的元组可供扫描
      return false;
    }
    // 将当前迭代器指向的元组赋值给输出参数 tuple，并获取其行标识符（RID）
    // 然后，递增表迭代器以指向下一个元组
    *tuple = *table_iter_;
    *rid = tuple->GetRid();
    ++table_iter_;
    // 检查当前元组是否满足过滤条件
    // 不满足过滤条件，则继续循环，直到找到满足条件的元组或迭代器到达末尾
  } while (plan_->filter_predicate_ != nullptr &&
           !plan_->filter_predicate_->Evaluate(tuple, table_info_->schema_).GetAs<bool>());

  // 检查当前事务的隔离级别是否为 READ_UNCOMMITTED
  // 如果不是，则需要获取行的共享锁，以确保在读取数据时不会发生并发写操作
  if (exec_ctx_->GetTransaction()->GetIsolationLevel() != IsolationLevel::READ_UNCOMMITTED) {
    // 尝试获取行的共享锁
    try {
      // 调用锁管理器的 LockRow 方法，传入当前事务、锁模式、表的 OID 和行的 RID
      bool is_locked = exec_ctx_->GetLockManager()->LockRow(exec_ctx_->GetTransaction(), LockManager::LockMode::SHARED,
                                                            table_info_->oid_, *rid);
      if (!is_locked) {
        // throw ExecutionException("SeqScan Executor Get Table Lock Failed");
      }
    } catch (TransactionAbortException &e) {
      // throw ExecutionException("SeqScan Executor Get Row Lock Failed" + e.GetInfo());
    }
  }
  // 为什么是先扫描再获取锁？
  // 通过先扫描再获取锁，可以减少不必要的锁操作，只对满足条件的元组进行锁定
  // 先扫描再获取锁可以减少锁的持有时间，从而降低死锁的可能性

  return true;
}

}  // namespace bustub
