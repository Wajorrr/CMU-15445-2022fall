//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// table_heap.h
//
// Identification: src/include/storage/table/table_heap.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "buffer/buffer_pool_manager.h"
#include "recovery/log_manager.h"
#include "storage/page/table_page.h"
#include "storage/table/table_iterator.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * TableHeap represents a physical table on disk.
 * This is just a doubly-linked list of pages.
 */
// TableHeap 类表示磁盘上的物理表，它实际上是一个双向链表结构的页面集合
// 这个类的主要功能是管理表的数据存储和操作
class TableHeap {
  // 友元类 TableIterator
  friend class TableIterator;

 public:
  ~TableHeap() = default;

  /**
   * Create a table heap without a transaction. (open table)
   * @param buffer_pool_manager the buffer pool manager
   * @param lock_manager the lock manager
   * @param log_manager the log manager
   * @param first_page_id the id of the first page
   */
  // 用于在没有事务的情况下创建一个表堆（打开表）
  TableHeap(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager, LogManager *log_manager,
            page_id_t first_page_id);

  /**
   * Create a table heap with a transaction. (create table)
   * @param buffer_pool_manager the buffer pool manager
   * @param lock_manager the lock manager
   * @param log_manager the log manager
   * @param txn the creating transaction
   */
  // 用于在有事务的情况下创建一个表堆（创建表）
  TableHeap(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager, LogManager *log_manager,
            Transaction *txn);

  /**
   * Insert a tuple into the table. If the tuple is too large (>= page_size), return false.
   * @param tuple tuple to insert
   * @param[out] rid the rid of the inserted tuple
   * @param txn the transaction performing the insert
   * @return true iff the insert is successful
   */
  // InsertTuple 方法用于向表中插入一个元组。如果元组太大（大于等于页面大小），则返回 false
  // 三个参数：要插入的元组 tuple、插入后元组的资源 ID rid 和执行插入操作的事务 txn
  auto InsertTuple(const Tuple &tuple, RID *rid, Transaction *txn) -> bool;

  /**
   * Mark the tuple as deleted. The actual delete will occur when ApplyDelete is called.
   * @param rid resource id of the tuple of delete
   * @param txn transaction performing the delete
   * @return true iff the delete is successful (i.e the tuple exists)
   */
  // 标记一个元组为已删除
  // 实际的删除操作将在调用 ApplyDelete 方法时执行
  auto MarkDelete(const RID &rid, Transaction *txn) -> bool;  // for delete

  /**
   * if the new tuple is too large to fit in the old page, return false (will delete and insert)
   * @param tuple new tuple
   * @param rid rid of the old tuple
   * @param txn transaction performing the update
   * @return true is update is successful.
   */
  // 用于更新一个元组
  // 三个参数：新元组 tuple、旧元组的资源 ID rid 和执行更新操作的事务 txn
  auto UpdateTuple(const Tuple &tuple, const RID &rid, Transaction *txn) -> bool;

  /**
   * Called on Commit/Abort to actually delete a tuple or rollback an insert.
   * @param rid rid of the tuple to delete
   * @param txn transaction performing the delete.
   */
  // 在提交或中止时调用，用于实际删除一个元组或回滚一个插入操作
  void ApplyDelete(const RID &rid, Transaction *txn);

  /**
   * Called on abort to rollback a delete.
   * @param rid rid of the deleted tuple.
   * @param txn transaction performing the rollback
   */
  // 在中止时调用，用于回滚一个删除操作
  void RollbackDelete(const RID &rid, Transaction *txn);

  /**
   * Read a tuple from the table.
   * @param rid rid of the tuple to read
   * @param tuple output variable for the tuple
   * @param txn transaction performing the read
   * @return true if the read was successful (i.e. the tuple exists)
   */
  // 用于从表中读取一个元组
  auto GetTuple(const RID &rid, Tuple *tuple, Transaction *txn, bool acquire_read_lock = true) -> bool;

  /** @return the begin iterator of this table */
  // 返回表的起始迭代器
  auto Begin(Transaction *txn) -> TableIterator;

  /** @return the end iterator of this table */
  // 返回表的结束迭代器
  auto End() -> TableIterator;

  /** @return the id of the first page of this table */
  // 返回表的第一个页面的 ID
  inline auto GetFirstPageId() const -> page_id_t { return first_page_id_; }

 private:
  BufferPoolManager *buffer_pool_manager_;
  LockManager *lock_manager_;
  LogManager *log_manager_;
  page_id_t first_page_id_{};
};

}  // namespace bustub
