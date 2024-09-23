//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// executor_context.h
//
// Identification: src/include/execution/executor_context.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <unordered_set>
#include <utility>
#include <vector>

#include "catalog/catalog.h"
#include "concurrency/transaction.h"
#include "storage/page/tmp_tuple_page.h"

namespace bustub {
/**
 * ExecutorContext stores all the context necessary to run an executor.
 */
// 用于存储执行器运行所需的所有上下文信息
class ExecutorContext {
 public:
  /**
   * Creates an ExecutorContext for the transaction that is executing the query.
   * @param transaction The transaction executing the query
   * @param catalog The catalog that the executor uses
   * @param bpm The buffer pool manager that the executor uses
   * @param txn_mgr The transaction manager that the executor uses
   * @param lock_mgr The lock manager that the executor uses
   */
  // 五个参数：transaction、catalog、bpm、txn_mgr 和 lock_mgr
  ExecutorContext(Transaction *transaction, Catalog *catalog, BufferPoolManager *bpm, TransactionManager *txn_mgr,
                  LockManager *lock_mgr)
      : transaction_(transaction), catalog_{catalog}, bpm_{bpm}, txn_mgr_(txn_mgr), lock_mgr_(lock_mgr) {}

  ~ExecutorContext() = default;

  DISALLOW_COPY_AND_MOVE(ExecutorContext);

  /** @return the running transaction */
  // 返回当前运行的事务
  auto GetTransaction() const -> Transaction * { return transaction_; }

  /** @return the catalog */
  // 返回数据库目录
  auto GetCatalog() -> Catalog * { return catalog_; }

  /** @return the buffer pool manager */
  // 返回缓冲池管理器
  auto GetBufferPoolManager() -> BufferPoolManager * { return bpm_; }

  /** @return the log manager - don't worry about it for now */
  // 返回日志管理器
  auto GetLogManager() -> LogManager * { return nullptr; }

  /** @return the lock manager */
  // 返回锁管理器
  auto GetLockManager() -> LockManager * { return lock_mgr_; }

  /** @return the transaction manager */
  // 返回事务管理器
  auto GetTransactionManager() -> TransactionManager * { return txn_mgr_; }

 private:
  /** The transaction context associated with this executor context */
  Transaction *transaction_;
  /** The datbase catalog associated with this executor context */
  Catalog *catalog_;
  /** The buffer pool manager associated with this executor context */
  BufferPoolManager *bpm_;
  /** The transaction manager associated with this executor context */
  TransactionManager *txn_mgr_;
  /** The lock manager associated with this executor context */
  LockManager *lock_mgr_;
};

}  // namespace bustub
