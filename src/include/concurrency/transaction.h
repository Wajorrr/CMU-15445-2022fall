//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// transaction.h
//
// Identification: src/include/concurrency/transaction.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <thread>  // NOLINT
#include <unordered_map>
#include <unordered_set>

#include "common/config.h"
#include "common/logger.h"
#include "storage/page/page.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * Transaction states for 2PL:
 *
 *     _________________________
 *    |                         v
 * GROWING -> SHRINKING -> COMMITTED   ABORTED
 *    |__________|________________________^
 *
 * Transaction states for Non-2PL:
 *     __________
 *    |          v
 * GROWING  -> COMMITTED     ABORTED
 *    |_________________________^
 *
 **/
// 定义了与事务管理相关的多个类和枚举类型
// 主要包括事务状态、隔离级别、写操作类型、写记录、事务异常和事务本身
// 事务的四种状态：GROWING（增长）、SHRINKING（收缩）、COMMITTED（已提交）和ABORTED（已中止）
// 用于表示事务在两阶段锁协议（2PL）和非两阶段锁协议中的不同阶段
enum class TransactionState { GROWING, SHRINKING, COMMITTED, ABORTED };

/**
 * Transaction isolation level.
 */
// 定义了事务的三种隔离级别：
// READ_UNCOMMITTED（读未提交）、REPEATABLE_READ（可重复读）和READ_COMMITTED（读已提交）
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED };

/**
 * Type of write operation.
 */
// 三种写操作类型：INSERT（插入）、DELETE（删除）和UPDATE（更新）
enum class WType { INSERT = 0, DELETE, UPDATE };

class TableHeap;
class Catalog;
using table_oid_t = uint32_t;
using index_oid_t = uint32_t;

/**
 * WriteRecord tracks information related to a write.
 */
// 用于跟踪与表写操作相关的信息
// 包含四个成员变量：rid_（行标识符）、wtype_（写操作类型）、tuple_（元组）和table_（表堆指针）
class TableWriteRecord {
 public:
  TableWriteRecord(RID rid, WType wtype, const Tuple &tuple, TableHeap *table)
      : rid_(rid), wtype_(wtype), tuple_(tuple), table_(table) {}

  RID rid_;      // 被写入的元组的行标识符
  WType wtype_;  // 写操作的类型，可以是插入（INSERT）、删除（DELETE）或更新（UPDATE）
  /** The tuple is only used for the update operation. */
  Tuple tuple_;  // 一个 Tuple 对象，表示与写操作相关的元组，仅在更新操作中使用
  /** The table heap specifies which table this write record is for. */
  TableHeap *table_;  // 一个指向 TableHeap 对象的指针，表示该写记录所属的表堆
};

/**
 * WriteRecord tracks information related to a write.
 */
// 用于跟踪与索引写操作相关的信息
class IndexWriteRecord {
 public:
  IndexWriteRecord(RID rid, table_oid_t table_oid, WType wtype, const Tuple &tuple, index_oid_t index_oid,
                   Catalog *catalog)
      : rid_(rid), table_oid_(table_oid), wtype_(wtype), tuple_(tuple), index_oid_(index_oid), catalog_(catalog) {}

  /** The rid is the value stored in the index. */
  RID rid_;  // 行标识符
  /** Table oid. */
  table_oid_t table_oid_;  // 表对象标识符
  /** Write type. */
  WType wtype_;  // 写操作类型
  /** The tuple is used to construct an index key. */
  Tuple tuple_;  // 元组
  /** The old tuple is only used for the update operation. */
  Tuple old_tuple_;  // 旧元组，仅在更新操作中使用
  /** Each table has an index list, this is the identifier of an index into the list. */
  index_oid_t index_oid_;  // 索引对象标识符
  /** The catalog contains metadata required to locate index. */
  Catalog *catalog_;  // 目录指针
};

/**
 * Reason to a transaction abortion
 */
// 八种事务中止的原因
enum class AbortReason {
  LOCK_ON_SHRINKING,                     // 事务在收缩状态下无法获取锁
  UPGRADE_CONFLICT,                      // 事务在等待升级锁时发生冲突
  LOCK_SHARED_ON_READ_UNCOMMITTED,       // 事务在读未提交隔离级别下共享锁
  TABLE_LOCK_NOT_PRESENT,                // 事务未持有表锁
  ATTEMPTED_INTENTION_LOCK_ON_ROW,       // 事务在行上尝试意向锁
  TABLE_UNLOCKED_BEFORE_UNLOCKING_ROWS,  // 事务在解锁行之前解锁表
  INCOMPATIBLE_UPGRADE,                  // 事务尝试的锁升级是不兼容的
  ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD      // 事务尝试解锁但未持有锁
};

/**
 * TransactionAbortException is thrown when state of a transaction is changed to ABORTED
 */
// 用于在事务状态变为 ABORTED 时抛出异常
class TransactionAbortException : public std::exception {
  txn_id_t txn_id_;
  AbortReason abort_reason_;

 public:
  explicit TransactionAbortException(txn_id_t txn_id, AbortReason abort_reason)
      : txn_id_(txn_id), abort_reason_(abort_reason) {}
  auto GetTransactionId() -> txn_id_t { return txn_id_; }
  auto GetAbortReason() -> AbortReason { return abort_reason_; }
  auto GetInfo() -> std::string {
    switch (abort_reason_) {
      case AbortReason::LOCK_ON_SHRINKING:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because it can not take locks in the shrinking state\n";
      case AbortReason::UPGRADE_CONFLICT:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because another transaction is already waiting to upgrade its lock\n";
      case AbortReason::LOCK_SHARED_ON_READ_UNCOMMITTED:
        return "Transaction " + std::to_string(txn_id_) + " aborted on lockshared on READ_UNCOMMITTED\n";
      case AbortReason::TABLE_LOCK_NOT_PRESENT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because table lock not present\n";
      case AbortReason::ATTEMPTED_INTENTION_LOCK_ON_ROW:
        return "Transaction " + std::to_string(txn_id_) + " aborted because intention lock attempted on row\n";
      case AbortReason::TABLE_UNLOCKED_BEFORE_UNLOCKING_ROWS:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because table locks dropped before dropping row locks\n";
      case AbortReason::INCOMPATIBLE_UPGRADE:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted lock upgrade is incompatible\n";
      case AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted to unlock but no lock held \n";
    }
    // Todo: Should fail with unreachable.
    return "";
  }
};

/**
 * Transaction tracks information related to a transaction.
 */
// 用于跟踪与事务相关的信息
class Transaction {
 public:
  // 接受两个参数：事务 ID（txn_id）和隔离级别（isolation_level），默认隔离级别为 REPEATABLE_READ
  explicit Transaction(txn_id_t txn_id, IsolationLevel isolation_level = IsolationLevel::REPEATABLE_READ)
      : isolation_level_(isolation_level),
        thread_id_(std::this_thread::get_id()),
        txn_id_(txn_id),
        prev_lsn_(INVALID_LSN),
        shared_lock_set_{new std::unordered_set<RID>},
        exclusive_lock_set_{new std::unordered_set<RID>},
        s_table_lock_set_{new std::unordered_set<table_oid_t>},
        x_table_lock_set_{new std::unordered_set<table_oid_t>},
        is_table_lock_set_{new std::unordered_set<table_oid_t>},
        ix_table_lock_set_{new std::unordered_set<table_oid_t>},
        six_table_lock_set_{new std::unordered_set<table_oid_t>},
        s_row_lock_set_{new std::unordered_map<table_oid_t, std::unordered_set<RID>>},
        x_row_lock_set_{new std::unordered_map<table_oid_t, std::unordered_set<RID>>} {
    // Initialize the sets that will be tracked.
    table_write_set_ = std::make_shared<std::deque<TableWriteRecord>>();
    index_write_set_ = std::make_shared<std::deque<IndexWriteRecord>>();
    page_set_ = std::make_shared<std::deque<bustub::Page *>>();
    deleted_page_set_ = std::make_shared<std::unordered_set<page_id_t>>();
  }

  ~Transaction() = default;

  DISALLOW_COPY(Transaction);

  /** @return the id of the thread running the transaction */
  // 返回运行事务的线程 ID
  inline auto GetThreadId() const -> std::thread::id { return thread_id_; }

  /** @return the id of this transaction */
  // 返回事务 ID
  inline auto GetTransactionId() const -> txn_id_t { return txn_id_; }

  /** @return the isolation level of this transaction */
  // 返回事务的隔离级别
  inline auto GetIsolationLevel() const -> IsolationLevel { return isolation_level_; }

  /** @return the list of table write records of this transaction */
  // 返回事务的表写记录集合
  inline auto GetWriteSet() -> std::shared_ptr<std::deque<TableWriteRecord>> { return table_write_set_; }

  /** @return the list of index write records of this transaction */
  // 返回事务的索引写记录集合
  inline auto GetIndexWriteSet() -> std::shared_ptr<std::deque<IndexWriteRecord>> { return index_write_set_; }

  /** @return the page set */
  // 返回事务的页面集合
  inline auto GetPageSet() -> std::shared_ptr<std::deque<Page *>> { return page_set_; }

  /**
   * Adds a tuple write record into the table write set.
   * @param write_record write record to be added
   */
  // 向表写记录集合中添加一个写记录
  inline void AppendTableWriteRecord(const TableWriteRecord &write_record) {
    table_write_set_->push_back(write_record);
  }

  /**
   * Adds an index write record into the index write set.
   * @param write_record write record to be added
   */
  // 向索引写记录集合中添加一个写记录
  inline void AppendIndexWriteRecord(const IndexWriteRecord &write_record) {
    index_write_set_->push_back(write_record);
  }

  /**
   * Adds a page into the page set.
   * @param page page to be added
   */
  // 向页面集合中添加一个页面
  inline void AddIntoPageSet(Page *page) {
    page_set_->push_back(page);
    // std::cout << "\n\nAddIntoPageSet:page=" << page->GetPageId() << "\n\n";
  }

  /** @return the deleted page set */
  // 返回已删除页面集合
  inline auto GetDeletedPageSet() -> std::shared_ptr<std::unordered_set<page_id_t>> { return deleted_page_set_; }

  /**
   * Adds a page to the deleted page set.
   * @param page_id id of the page to be marked as deleted
   */
  // 向已删除页面集合中添加一个页面 ID
  inline void AddIntoDeletedPageSet(page_id_t page_id) {
    deleted_page_set_->insert(page_id);
    // std::cout << "\n\nAddIntoDeletedPageSet:page=" << page_id << "\n\n";
  }

  /** @return the set of resources under a shared lock */
  // 返回共享锁集合
  inline auto GetSharedLockSet() -> std::shared_ptr<std::unordered_set<RID>> { return shared_lock_set_; }

  /** @return the set of rows under a shared lock */
  // 返回共享行锁集合
  inline auto GetSharedRowLockSet() -> std::shared_ptr<std::unordered_map<table_oid_t, std::unordered_set<RID>>> {
    return s_row_lock_set_;
  }

  /** @return the set of resources under an exclusive lock */
  // 返回排他锁集合
  inline auto GetExclusiveLockSet() -> std::shared_ptr<std::unordered_set<RID>> { return exclusive_lock_set_; }

  /** @return the set of rows in under an exclusive lock */
  // 返回排他行锁集合
  inline auto GetExclusiveRowLockSet() -> std::shared_ptr<std::unordered_map<table_oid_t, std::unordered_set<RID>>> {
    return x_row_lock_set_;
  }

  /** @return the set of resources under a shared lock */
  // 返回共享表锁集合
  inline auto GetSharedTableLockSet() -> std::shared_ptr<std::unordered_set<table_oid_t>> { return s_table_lock_set_; }
  // 返回排他表锁集合
  inline auto GetExclusiveTableLockSet() -> std::shared_ptr<std::unordered_set<table_oid_t>> {
    return x_table_lock_set_;
  }
  // 返回意向共享表锁集合
  inline auto GetIntentionSharedTableLockSet() -> std::shared_ptr<std::unordered_set<table_oid_t>> {
    return is_table_lock_set_;
  }
  // 返回意向排他表锁集合
  inline auto GetIntentionExclusiveTableLockSet() -> std::shared_ptr<std::unordered_set<table_oid_t>> {
    return ix_table_lock_set_;
  }
  // 返回共享意向排他表锁集合
  inline auto GetSharedIntentionExclusiveTableLockSet() -> std::shared_ptr<std::unordered_set<table_oid_t>> {
    return six_table_lock_set_;
  }

  /** @return true if rid (belong to table oid) is shared locked by this transaction */
  // 检查指定的行是否被共享锁定
  auto IsRowSharedLocked(const table_oid_t &oid, const RID &rid) -> bool {
    auto row_lock_set = s_row_lock_set_->find(oid);
    if (row_lock_set == s_row_lock_set_->end()) {
      return false;
    }
    return row_lock_set->second.find(rid) != row_lock_set->second.end();
  }

  /** @return true if rid (belong to table oid) is exclusive locked by this transaction */
  // 检查指定的行是否被排他锁定
  auto IsRowExclusiveLocked(const table_oid_t &oid, const RID &rid) -> bool {
    auto row_lock_set = x_row_lock_set_->find(oid);
    if (row_lock_set == x_row_lock_set_->end()) {
      return false;
    }
    return row_lock_set->second.find(rid) != row_lock_set->second.end();
  }

  // 检查指定的表是否被意向共享锁定
  auto IsTableIntentionSharedLocked(const table_oid_t &oid) -> bool {
    return is_table_lock_set_->find(oid) != is_table_lock_set_->end();
  }

  // 检查指定的表是否被共享锁定
  auto IsTableSharedLocked(const table_oid_t &oid) -> bool {
    return s_table_lock_set_->find(oid) != s_table_lock_set_->end();
  }

  // 检查指定的表是否被意向排他锁定
  auto IsTableIntentionExclusiveLocked(const table_oid_t &oid) -> bool {
    return ix_table_lock_set_->find(oid) != ix_table_lock_set_->end();
  }

  // 检查指定的表是否被排他锁定
  auto IsTableExclusiveLocked(const table_oid_t &oid) -> bool {
    return x_table_lock_set_->find(oid) != x_table_lock_set_->end();
  }

  // 检查指定的表是否被共享意向排他锁定
  auto IsTableSharedIntentionExclusiveLocked(const table_oid_t &oid) -> bool {
    return six_table_lock_set_->find(oid) != six_table_lock_set_->end();
  }

  /** @return the current state of the transaction */
  // 返回事务的当前状态
  inline auto GetState() -> TransactionState { return state_; }

  // 锁定事务
  inline auto LockTxn() -> void { latch_.lock(); }

  // 解锁事务
  inline auto UnlockTxn() -> void { latch_.unlock(); }

  /**
   * Set the state of the transaction.
   * @param state new state
   */
  // 设置事务的状态
  inline void SetState(TransactionState state) { state_ = state; }

  /** @return the previous LSN */
  // 返回前一个 LSN
  inline auto GetPrevLSN() -> lsn_t { return prev_lsn_; }

  /**
   * Set the previous LSN.
   * @param prev_lsn new previous lsn
   */
  // 设置前一个 LSN
  inline void SetPrevLSN(lsn_t prev_lsn) { prev_lsn_ = prev_lsn; }

 private:
  /** The current transaction state. */
  // 存储当前事务的状态，类型为 TransactionState。初始状态为 GROWING，表示事务正在进行中
  TransactionState state_{TransactionState::GROWING};
  /** The isolation level of the transaction. */
  // 存储事务的隔离级别
  IsolationLevel isolation_level_;
  /** The thread ID, used in single-threaded transactions. */
  // 存储执行该事务的线程 ID
  std::thread::id thread_id_;
  /** The ID of this transaction. */
  // 存储事务的唯一标识符
  txn_id_t txn_id_;

  /** The undo set of table tuples. */
  // 存储表元组的撤销记录
  std::shared_ptr<std::deque<TableWriteRecord>> table_write_set_;
  /** The undo set of indexes. */
  // 存储索引的撤销记录
  std::shared_ptr<std::deque<IndexWriteRecord>> index_write_set_;
  /** The LSN of the last record written by the transaction. */
  // 存储事务最后写入记录的日志序列号（LSN）
  lsn_t prev_lsn_;

  // 用于并发控制的互斥锁
  std::mutex latch_;

  /** Concurrent index: the pages that were latched during index operation. */
  // 存储在索引操作期间锁定的页面
  std::shared_ptr<std::deque<Page *>> page_set_;
  /** Concurrent index: the page IDs that were deleted during index operation.*/
  // 存储在索引操作期间删除的页面 ID
  std::shared_ptr<std::unordered_set<page_id_t>> deleted_page_set_;

  /** LockManager: the set of shared-locked tuples held by this transaction. */
  // 存储事务持有的共享锁的元组集合
  std::shared_ptr<std::unordered_set<RID>> shared_lock_set_;
  /** LockManager: the set of exclusive-locked tuples held by this transaction. */
  // 存储事务持有的排他锁的元组集合
  std::shared_ptr<std::unordered_set<RID>> exclusive_lock_set_;

  // 分别存储事务持有的共享表锁、排他表锁、意向共享表锁、意向排他表锁和共享意向排他表锁的集合
  /** LockManager: the set of table locks held by this transaction. */
  std::shared_ptr<std::unordered_set<table_oid_t>> s_table_lock_set_;
  std::shared_ptr<std::unordered_set<table_oid_t>> x_table_lock_set_;
  std::shared_ptr<std::unordered_set<table_oid_t>> is_table_lock_set_;
  std::shared_ptr<std::unordered_set<table_oid_t>> ix_table_lock_set_;
  std::shared_ptr<std::unordered_set<table_oid_t>> six_table_lock_set_;

  /** LockManager: the set of row locks held by this transaction. */
  // 分别存储事务持有的共享行锁和排他行锁的集合
  std::shared_ptr<std::unordered_map<table_oid_t, std::unordered_set<RID>>> s_row_lock_set_;
  std::shared_ptr<std::unordered_map<table_oid_t, std::unordered_set<RID>>> x_row_lock_set_;
};

}  // namespace bustub
