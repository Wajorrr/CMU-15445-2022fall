//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// table_page.cpp
//
// Identification: src/storage/page/table_page.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/table_page.h"

#include <cassert>

namespace bustub {

void TablePage::Init(page_id_t page_id, uint32_t page_size, page_id_t prev_page_id, LogManager *log_manager,
                     Transaction *txn) {
  // Set the page ID.
  memcpy(GetData(), &page_id, sizeof(page_id));
  // Log that we are creating a new page.
  if (enable_logging) {
    LogRecord log_record =
        LogRecord(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::NEWPAGE, prev_page_id, page_id);
    lsn_t lsn = log_manager->AppendLogRecord(&log_record);
    SetLSN(lsn);
    txn->SetPrevLSN(lsn);
  }
  // Set the previous and next page IDs.
  SetPrevPageId(prev_page_id);
  SetNextPageId(INVALID_PAGE_ID);
  SetFreeSpacePointer(page_size);
  SetTupleCount(0);
}

// 用于将一个元组插入到表页中
auto TablePage::InsertTuple(const Tuple &tuple, RID *rid, Transaction *txn, LockManager *lock_manager,
                            LogManager *log_manager) -> bool {
  BUSTUB_ASSERT(tuple.size_ > 0, "Cannot have empty tuples.");
  // If there is not enough space, then return false.
  // 检查当前页面是否有足够的空闲空间来容纳新的元组
  if (GetFreeSpaceRemaining() < tuple.size_ + SIZE_TUPLE) {
    return false;
  }

  // Try to find a free slot to reuse.
  // 尝试找到一个可以重用的空插槽
  uint32_t i;
  for (i = 0; i < GetTupleCount(); i++) {
    // If the slot is empty, i.e. its tuple has size 0,
    if (GetTupleSize(i) == 0) {
      // Then we break out of the loop at index i.
      break;
    }
  }

  // If there was no free slot left, and we cannot claim it from the free space, then we give up.
  // 如果没有找到空插槽，并且剩余的空闲空间不足以容纳新的元组，函数返回 false，表示插入失败
  if (i == GetTupleCount() && GetFreeSpaceRemaining() < tuple.size_ + SIZE_TUPLE) {
    return false;
  }

  // Otherwise we claim available free space.
  // 如果有足够的空闲空间，函数会更新空闲空间指针，减少相应的元组大小
  // 将元组数据复制到空闲空间的末尾
  SetFreeSpacePointer(GetFreeSpacePointer() - tuple.size_);
  memcpy(GetData() + GetFreeSpacePointer(), tuple.data_, tuple.size_);

  // Set the tuple.
  // 设置元组的偏移量和大小
  // i为第i个插槽，GetFreeSpacePointer()为新插入元组的偏移量
  SetTupleOffsetAtSlot(i, GetFreeSpacePointer());
  SetTupleSize(i, tuple.size_);

  // 更新 RID 对象，使其指向新插入的元组
  rid->Set(GetTablePageId(), i);
  // 如果插入的是一个新的元组（而不是重用的插槽），函数会增加元组计数
  if (i == GetTupleCount()) {
    SetTupleCount(GetTupleCount() + 1);
  }

  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  // Write the log record.
  //  if (enable_logging) {
  //    BUSTUB_ASSERT(!txn->IsSharedLocked(*rid) && !txn->IsExclusiveLocked(*rid), "A new tuple should not be locked.");
  //    // Acquire an exclusive lock on the new tuple.
  //    bool locked = lock_manager->LockExclusive(txn, *rid);
  //    BUSTUB_ENSURE(locked, "Locking a new tuple should always work.");
  //    LogRecord log_record(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::INSERT, *rid, tuple);
  //    lsn_t lsn = log_manager->AppendLogRecord(&log_record);
  //    SetLSN(lsn);
  //    txn->SetPrevLSN(lsn);
  //  }
  return true;
}

auto TablePage::MarkDelete(const RID &rid, Transaction *txn, LockManager *lock_manager,
                           LogManager *log_manager) -> bool {
  uint32_t slot_num = rid.GetSlotNum();
  // If the slot number is invalid, abort the transaction.
  // 如果插槽号无效，函数返回 false，表示删除失败
  if (slot_num >= GetTupleCount()) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }

  uint32_t tuple_size = GetTupleSize(slot_num);
  // If the tuple is already deleted, abort the transaction.
  // 如果元组已经被删除，函数返回 false，表示删除失败
  if (IsDeleted(tuple_size)) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }

  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  if (enable_logging) {
  //    // Acquire an exclusive lock, upgrading from a shared lock if necessary.
  //    if (txn->IsSharedLocked(rid)) {
  //      if (!lock_manager->LockUpgrade(txn, rid)) {
  //        return false;
  //      }
  //    } else if (!txn->IsExclusiveLocked(rid) && !lock_manager->LockExclusive(txn, rid)) {
  //      return false;
  //    }
  //    Tuple dummy_tuple;
  //    LogRecord log_record(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::MARKDELETE, rid, dummy_tuple);
  //    lsn_t lsn = log_manager->AppendLogRecord(&log_record);
  //    SetLSN(lsn);
  //    txn->SetPrevLSN(lsn);
  //  }

  // Mark the tuple as deleted.
  // 将元组大小中的删除标志位设置为 1
  if (tuple_size > 0) {
    SetTupleSize(slot_num, SetDeletedFlag(tuple_size));
  }
  return true;
}

auto TablePage::UpdateTuple(const Tuple &new_tuple, Tuple *old_tuple, const RID &rid, Transaction *txn,
                            LockManager *lock_manager, LogManager *log_manager) -> bool {
  BUSTUB_ASSERT(new_tuple.size_ > 0, "Cannot have empty tuples.");
  uint32_t slot_num = rid.GetSlotNum();
  // If the slot number is invalid, abort the transaction.
  // 如果插槽号无效，函数返回 false，表示更新失败
  if (slot_num >= GetTupleCount()) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }
  uint32_t tuple_size = GetTupleSize(slot_num);
  // If the tuple is deleted, abort the transaction.
  // 如果元组已经被删除，函数返回 false，表示更新失败
  if (IsDeleted(tuple_size)) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }
  // If there is not enough space to update, we need to update via delete followed by an insert (not enough space).
  // 如果没有足够的空间来更新元组，函数返回 false，表示更新失败
  if (GetFreeSpaceRemaining() + tuple_size < new_tuple.size_) {
    return false;
  }

  // Copy out the old value.
  // 将旧元组的数据复制到 old_tuple 中
  // 获取旧元组的偏移量
  uint32_t tuple_offset = GetTupleOffsetAtSlot(slot_num);
  old_tuple->size_ = tuple_size;
  if (old_tuple->allocated_) {
    delete[] old_tuple->data_;
  }
  old_tuple->data_ = new char[old_tuple->size_];
  // 根据偏移量和大小，将旧元组的数据复制到 old_tuple 中
  memcpy(old_tuple->data_, GetData() + tuple_offset, old_tuple->size_);
  old_tuple->rid_ = rid;
  old_tuple->allocated_ = true;

  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  if (enable_logging) {
  //    // Acquire an exclusive lock, upgrading from shared if necessary.
  //    if (txn->IsSharedLocked(rid)) {
  //      if (!lock_manager->LockUpgrade(txn, rid)) {
  //        return false;
  //      }
  //    } else if (!txn->IsExclusiveLocked(rid) && !lock_manager->LockExclusive(txn, rid)) {
  //      return false;
  //    }
  //    LogRecord log_record(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::UPDATE, rid, *old_tuple,
  //    new_tuple); lsn_t lsn = log_manager->AppendLogRecord(&log_record); SetLSN(lsn); txn->SetPrevLSN(lsn);
  //  }

  // Perform the update.
  // 更新元组的数据
  // 获取当前空闲空间指针的位置
  uint32_t free_space_pointer = GetFreeSpacePointer();
  BUSTUB_ASSERT(tuple_offset >= free_space_pointer, "Offset should appear after current free space position.");
  // 使用 memmove 函数将空闲空间指针之后的数据向后移动，以腾出足够的空间来容纳新的元组数据
  memmove(GetData() + free_space_pointer + tuple_size - new_tuple.size_, GetData() + free_space_pointer,
          tuple_offset - free_space_pointer);
  // 更新空闲空间指针
  SetFreeSpacePointer(free_space_pointer + tuple_size - new_tuple.size_);
  // 将新元组的数据复制到空闲空间的末尾
  memcpy(GetData() + tuple_offset + tuple_size - new_tuple.size_, new_tuple.data_, new_tuple.size_);
  SetTupleSize(slot_num, new_tuple.size_);

  // Update all tuple offsets.
  // 更新所有元组的偏移量
  for (uint32_t i = 0; i < GetTupleCount(); ++i) {
    uint32_t tuple_offset_i = GetTupleOffsetAtSlot(i);
    if (GetTupleSize(i) > 0 && tuple_offset_i < tuple_offset + tuple_size) {
      SetTupleOffsetAtSlot(i, tuple_offset_i + tuple_size - new_tuple.size_);
    }
  }
  return true;
}

// 用于在表页中应用删除操作
void TablePage::ApplyDelete(const RID &rid, Transaction *txn, LogManager *log_manager) {
  // 获取指定 RID 的插槽编号
  uint32_t slot_num = rid.GetSlotNum();
  BUSTUB_ASSERT(slot_num < GetTupleCount(), "Cannot have more slots than tuples.");

  // 获取该插槽对应的元组偏移量和元组大小
  uint32_t tuple_offset = GetTupleOffsetAtSlot(slot_num);
  uint32_t tuple_size = GetTupleSize(slot_num);
  // Check if this is a delete operation, i.e. commit a delete.
  // 检查该元组是否已被标记为删除
  if (IsDeleted(tuple_size)) {
    // 如果是，则取消删除标记并更新元组大小。否则，表示这是一个插入操作的回滚
    tuple_size = UnsetDeletedFlag(tuple_size);
  }
  // Otherwise we are rolling back an insert.

  // We need to copy out the deleted tuple for undo purposes.
  // 复制被删除的元组数据
  Tuple delete_tuple;
  delete_tuple.size_ = tuple_size;
  delete_tuple.data_ = new char[delete_tuple.size_];
  memcpy(delete_tuple.data_, GetData() + tuple_offset, delete_tuple.size_);
  delete_tuple.rid_ = rid;
  delete_tuple.allocated_ = true;

  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  if (enable_logging) {
  //    BUSTUB_ASSERT(txn->IsExclusiveLocked(rid), "We must own the exclusive lock!");
  //
  //    LogRecord log_record(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::APPLYDELETE, rid, delete_tuple);
  //    lsn_t lsn = log_manager->AppendLogRecord(&log_record);
  //    SetLSN(lsn);
  //    txn->SetPrevLSN(lsn);
  //  }

  // 更新空闲空间指针的位置
  // 并通过 memmove 函数将空闲空间指针之后的数据向后移动，以腾出足够的空间来容纳新的元组数据
  uint32_t free_space_pointer = GetFreeSpacePointer();
  BUSTUB_ASSERT(tuple_offset >= free_space_pointer, "Free space appears before tuples.");

  memmove(GetData() + free_space_pointer + tuple_size, GetData() + free_space_pointer,
          tuple_offset - free_space_pointer);
  // 更新空闲空间指针的位置，使其反映新的空闲空间位置，并将指定插槽的元组大小和偏移量设置为零
  SetFreeSpacePointer(free_space_pointer + tuple_size);
  SetTupleSize(slot_num, 0);
  SetTupleOffsetAtSlot(slot_num, 0);

  // Update all tuple offsets.
  // 遍历所有元组插槽，更新它们的偏移量
  for (uint32_t i = 0; i < GetTupleCount(); ++i) {
    uint32_t tuple_offset_i = GetTupleOffsetAtSlot(i);
    if (GetTupleSize(i) != 0 && tuple_offset_i < tuple_offset) {
      SetTupleOffsetAtSlot(i, tuple_offset_i + tuple_size);
    }
  }
}

void TablePage::RollbackDelete(const RID &rid, Transaction *txn, LogManager *log_manager) {
  // Log the rollback.
  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  if (enable_logging) {
  //    BUSTUB_ASSERT(txn->IsExclusiveLocked(rid), "We must own an exclusive lock on the RID.");
  //    Tuple dummy_tuple;
  //    LogRecord log_record(txn->GetTransactionId(), txn->GetPrevLSN(), LogRecordType::ROLLBACKDELETE, rid,
  //    dummy_tuple); lsn_t lsn = log_manager->AppendLogRecord(&log_record); SetLSN(lsn); txn->SetPrevLSN(lsn);
  //  }

  uint32_t slot_num = rid.GetSlotNum();
  BUSTUB_ASSERT(slot_num < GetTupleCount(), "We can't have more slots than tuples.");
  uint32_t tuple_size = GetTupleSize(slot_num);

  // Unset the deleted flag.
  // 取消元组的删除标记
  if (IsDeleted(tuple_size)) {
    SetTupleSize(slot_num, UnsetDeletedFlag(tuple_size));
  }
}

auto TablePage::GetTuple(const RID &rid, Tuple *tuple, Transaction *txn, LockManager *lock_manager) -> bool {
  // Get the current slot number.
  uint32_t slot_num = rid.GetSlotNum();
  // If somehow we have more slots than tuples, abort the transaction.
  // 如果插槽号无效，函数返回 false，表示获取元组失败
  if (slot_num >= GetTupleCount()) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }
  // Otherwise get the current tuple size too.
  uint32_t tuple_size = GetTupleSize(slot_num);
  // If the tuple is deleted, abort the transaction.
  // 如果元组已经被删除，函数返回 false，表示获取元组失败
  if (IsDeleted(tuple_size)) {
    if (enable_logging) {
      txn->SetState(TransactionState::ABORTED);
    }
    return false;
  }

  /**
   * Removed to support new lock manager API for p4 (multilevel locking); Big hack energy
   * This clause was used in logging and recovery projects previously; not being used right now
   */
  //  // Otherwise we have a valid tuple, try to acquire at least a shared lock.
  //  if (enable_logging) {
  //    if (!txn->IsSharedLocked(rid) && !txn->IsExclusiveLocked(rid) && !lock_manager->LockShared(txn, rid)) {
  //      return false;
  //    }
  //  }

  // At this point, we have at least a shared lock on the RID. Copy the tuple data into our result.
  // 获取元组的偏移量
  uint32_t tuple_offset = GetTupleOffsetAtSlot(slot_num);
  tuple->size_ = tuple_size;
  if (tuple->allocated_) {
    delete[] tuple->data_;
  }
  tuple->data_ = new char[tuple->size_];
  // 根据偏移量和大小，将元组的数据复制到 tuple 中
  memcpy(tuple->data_, GetData() + tuple_offset, tuple->size_);
  tuple->rid_ = rid;
  tuple->allocated_ = true;
  return true;
}

auto TablePage::GetFirstTupleRid(RID *first_rid) -> bool {
  // Find and return the first valid tuple.
  for (uint32_t i = 0; i < GetTupleCount(); ++i) {
    // 找到第一个有效的元组，将其 RID 设置为 first_rid
    if (!IsDeleted(GetTupleSize(i))) {
      first_rid->Set(GetTablePageId(), i);
      return true;
    }
  }
  first_rid->Set(INVALID_PAGE_ID, 0);
  return false;
}

auto TablePage::GetNextTupleRid(const RID &cur_rid, RID *next_rid) -> bool {
  BUSTUB_ASSERT(cur_rid.GetPageId() == GetTablePageId(), "Wrong table!");
  // Find and return the first valid tuple after our current slot number.
  for (auto i = cur_rid.GetSlotNum() + 1; i < GetTupleCount(); ++i) {
    // 找到当前元组之后的第一个有效元组，将其 RID 设置为 next_rid
    if (!IsDeleted(GetTupleSize(i))) {
      next_rid->Set(GetTablePageId(), i);
      return true;
    }
  }
  // Otherwise return false as there are no more tuples.
  next_rid->Set(INVALID_PAGE_ID, 0);
  return false;
}
}  // namespace bustub
