//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// table_page.h
//
// Identification: src/include/storage/page/table_page.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstring>

#include "common/rid.h"
#include "concurrency/lock_manager.h"
#include "recovery/log_manager.h"
#include "storage/page/page.h"
#include "storage/table/tuple.h"

static constexpr uint64_t DELETE_MASK = (1U << (8 * sizeof(uint32_t) - 1));

namespace bustub {

/**
 * Slotted page format:
 *  ---------------------------------------------------------
 *  | HEADER | ... FREE SPACE ... | ... INSERTED TUPLES ... |
 *  ---------------------------------------------------------
 *                                ^
 *                                free space pointer
 *
 *  Header format (size in bytes):
 *  ----------------------------------------------------------------------------
 *  | PageId (4)| LSN (4)| PrevPageId (4)| NextPageId (4)| FreeSpacePointer(4) |
 *  ----------------------------------------------------------------------------
 *  ----------------------------------------------------------------
 *  | TupleCount (4) | Tuple_1 offset (4) | Tuple_1 size (4) | ... |
 *  ----------------------------------------------------------------
 *
 * 页头的格式如下：
    PageId (4 字节)：页的唯一标识符。
    LSN (4 字节)：日志序列号，用于恢复操作。
    PrevPageId (4 字节)：前一页的页 ID。
    NextPageId (4 字节)：后一页的页 ID。
    FreeSpacePointer (4 字节)：指向当前空闲空间的指针。
    TupleCount (4 字节)：元组的数量。
    Tuple_1 offset (4 字节)：第一个元组的偏移量。
    Tuple_1 size (4 字节)：第一个元组的大小。
 */
// TablePage 类是一个继承自 Page 类的类，用于表示数据库中的表页。
// 表页采用插槽页面格式，包含一个页头和插入的元组。
// 页头包含页的元数据，如页 ID、日志序列号（LSN）、前一页和后一页的页 ID、空闲空间指针等。
// 插槽页面格式的设计使得插入和删除操作更加高效
class TablePage : public Page {
 public:
  /**
   * Initialize the TablePage header.
   * @param page_id the page ID of this table page
   * @param page_size the size of this table page
   * @param prev_page_id the previous table page ID
   * @param log_manager the log manager in use
   * @param txn the transaction that this page is created in
   */
  // 用于初始化表页的页头，接受页 ID、页大小、前一页 ID、日志管理器和事务作为参数
  void Init(page_id_t page_id, uint32_t page_size, page_id_t prev_page_id, LogManager *log_manager, Transaction *txn);

  // GetTablePageId、GetPrevPageId 和 GetNextPageId 方法
  // 分别返回表页的页 ID、前一页的页 ID 和后一页的页 ID
  /** @return the page ID of this table page */
  auto GetTablePageId() -> page_id_t { return *reinterpret_cast<page_id_t *>(GetData()); }

  /** @return the page ID of the previous table page */
  auto GetPrevPageId() -> page_id_t { return *reinterpret_cast<page_id_t *>(GetData() + OFFSET_PREV_PAGE_ID); }

  /** @return the page ID of the next table page */
  auto GetNextPageId() -> page_id_t { return *reinterpret_cast<page_id_t *>(GetData() + OFFSET_NEXT_PAGE_ID); }

  // SetPrevPageId 和 SetNextPageId 方法用于设置前一页和后一页的页 ID
  /** Set the page id of the previous page in the table. */
  void SetPrevPageId(page_id_t prev_page_id) {
    memcpy(GetData() + OFFSET_PREV_PAGE_ID, &prev_page_id, sizeof(page_id_t));
  }

  /** Set the page id of the next page in the table. */
  void SetNextPageId(page_id_t next_page_id) {
    memcpy(GetData() + OFFSET_NEXT_PAGE_ID, &next_page_id, sizeof(page_id_t));
  }

  /**
   * Insert a tuple into the table.
   * @param tuple tuple to insert
   * @param[out] rid rid of the inserted tuple
   * @param txn transaction performing the insert
   * @param lock_manager the lock manager
   * @param log_manager the log manager
   * @return true if the insert is successful (i.e. there is enough space)
   */
  // 用于将一个元组插入表页
  auto InsertTuple(const Tuple &tuple, RID *rid, Transaction *txn, LockManager *lock_manager,
                   LogManager *log_manager) -> bool;

  /**
   * Mark a tuple as deleted. This does not actually delete the tuple.
   * @param rid rid of the tuple to mark as deleted
   * @param txn transaction performing the delete
   * @param lock_manager the lock manager
   * @param log_manager the log manager
   * @return true if marking the tuple as deleted is successful (i.e the tuple exists)
   */
  // 用于标记一个元组为删除
  auto MarkDelete(const RID &rid, Transaction *txn, LockManager *lock_manager, LogManager *log_manager) -> bool;

  /**
   * Update a tuple.
   * @param new_tuple new value of the tuple
   * @param[out] old_tuple old value of the tuple
   * @param rid rid of the tuple
   * @param txn transaction performing the update
   * @param lock_manager the lock manager
   * @param log_manager the log manager
   * @return true if updating the tuple succeeded
   */
  // 用于更新一个元组
  auto UpdateTuple(const Tuple &new_tuple, Tuple *old_tuple, const RID &rid, Transaction *txn,
                   LockManager *lock_manager, LogManager *log_manager) -> bool;

  /** To be called on commit or abort. Actually perform the delete or rollback an insert. */
  // 在提交或中止时调用，实际执行删除操作或回滚插入操作
  void ApplyDelete(const RID &rid, Transaction *txn, LogManager *log_manager);

  /** To be called on abort. Rollback a delete, i.e. this reverses a MarkDelete. */
  // 在中止时调用，回滚删除操作，即撤销 MarkDelete 操作
  void RollbackDelete(const RID &rid, Transaction *txn, LogManager *log_manager);

  /**
   * Read a tuple from a table.
   * @param rid rid of the tuple to read
   * @param[out] tuple the tuple that was read
   * @param txn transaction performing the read
   * @param lock_manager the lock manager
   * @return true if the read is successful (i.e. the tuple exists)
   */
  // 用于从表页中读取一个元组
  auto GetTuple(const RID &rid, Tuple *tuple, Transaction *txn, LockManager *lock_manager) -> bool;

  /** @return the rid of the first tuple in this page */

  /**
   * @param[out] first_rid the RID of the first tuple in this page
   * @return true if the first tuple exists, false otherwise
   */
  // 返回表页中第一个元组的 RID
  auto GetFirstTupleRid(RID *first_rid) -> bool;

  /**
   * @param cur_rid the RID of the current tuple
   * @param[out] next_rid the RID of the tuple following the current tuple
   * @return true if the next tuple exists, false otherwise
   */
  // 返回当前元组之后的下一个元组的 RID
  auto GetNextTupleRid(const RID &cur_rid, RID *next_rid) -> bool;

 private:
  static_assert(sizeof(page_id_t) == 4);

  static constexpr size_t SIZE_TABLE_PAGE_HEADER = 24;  // 表页头的大小
  static constexpr size_t SIZE_TUPLE = 8;               // 每个元组的大小
  static constexpr size_t OFFSET_PREV_PAGE_ID = 8;      // 前一页 ID 在页头中的偏移量
  static constexpr size_t OFFSET_NEXT_PAGE_ID = 12;     // 后一页 ID 在页头中的偏移量
  static constexpr size_t OFFSET_FREE_SPACE = 16;       // 空闲空间指针在页头中的偏移量
  static constexpr size_t OFFSET_TUPLE_COUNT = 20;      // 元组数量在页头中的偏移量
  static constexpr size_t OFFSET_TUPLE_OFFSET = 24;  // 元组偏移量在页头中的偏移量 // Naming things is hard.
  static constexpr size_t OFFSET_TUPLE_SIZE = 28;    // 元组大小在页头中的偏移量

  // GetFreeSpacePointer 和 SetFreeSpacePointer 方法用于获取和设置空闲空间指针
  /** @return pointer to the end of the current free space, see header comment */
  auto GetFreeSpacePointer() -> uint32_t { return *reinterpret_cast<uint32_t *>(GetData() + OFFSET_FREE_SPACE); }

  /** Sets the pointer, this should be the end of the current free space. */
  void SetFreeSpacePointer(uint32_t free_space_pointer) {
    memcpy(GetData() + OFFSET_FREE_SPACE, &free_space_pointer, sizeof(uint32_t));
  }

  /**
   * @note returned tuple count may be an overestimate because some slots may be empty
   * @return at least the number of tuples in this page
   */
  // GetTupleCount 和 SetTupleCount 方法用于获取和设置元组的数量
  auto GetTupleCount() -> uint32_t { return *reinterpret_cast<uint32_t *>(GetData() + OFFSET_TUPLE_COUNT); }

  /** Set the number of tuples in this page. */
  void SetTupleCount(uint32_t tuple_count) { memcpy(GetData() + OFFSET_TUPLE_COUNT, &tuple_count, sizeof(uint32_t)); }

  // 返回剩余的空闲空间
  auto GetFreeSpaceRemaining() -> uint32_t {
    return GetFreeSpacePointer() - SIZE_TABLE_PAGE_HEADER - SIZE_TUPLE * GetTupleCount();
  }

  /** @return tuple offset at slot slot_num */
  // 用于获取和设置指定插槽的元组偏移量
  auto GetTupleOffsetAtSlot(uint32_t slot_num) -> uint32_t {
    return *reinterpret_cast<uint32_t *>(GetData() + OFFSET_TUPLE_OFFSET + SIZE_TUPLE * slot_num);
  }

  /** Set tuple offset at slot slot_num. */
  void SetTupleOffsetAtSlot(uint32_t slot_num, uint32_t offset) {
    memcpy(GetData() + OFFSET_TUPLE_OFFSET + SIZE_TUPLE * slot_num, &offset, sizeof(uint32_t));
  }

  /** @return tuple size at slot slot_num */
  // 用于获取和设置指定插槽的元组大小
  auto GetTupleSize(uint32_t slot_num) -> uint32_t {
    return *reinterpret_cast<uint32_t *>(GetData() + OFFSET_TUPLE_SIZE + SIZE_TUPLE * slot_num);
  }

  /** Set tuple size at slot slot_num. */
  void SetTupleSize(uint32_t slot_num, uint32_t size) {
    memcpy(GetData() + OFFSET_TUPLE_SIZE + SIZE_TUPLE * slot_num, &size, sizeof(uint32_t));
  }

  /** @return true if the tuple is deleted or empty */
  // 用于判断元组是否被删除
  static auto IsDeleted(uint32_t tuple_size) -> bool {
    return static_cast<bool>(tuple_size & DELETE_MASK) || tuple_size == 0;
  }

  /** @return tuple size with the deleted flag set */
  // 用于设置和取消元组的删除标志
  static auto SetDeletedFlag(uint32_t tuple_size) -> uint32_t {
    return static_cast<uint32_t>(tuple_size | DELETE_MASK);
  }

  /** @return tuple size with the deleted flag unset */
  static auto UnsetDeletedFlag(uint32_t tuple_size) -> uint32_t {
    return static_cast<uint32_t>(tuple_size & (~DELETE_MASK));
  }
};
}  // namespace bustub
