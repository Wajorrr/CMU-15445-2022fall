//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lock_manager.cpp
//
// Identification: src/concurrency/lock_manager.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "concurrency/lock_manager.h"

#include "common/config.h"
#include "concurrency/transaction.h"
#include "concurrency/transaction_manager.h"

namespace bustub {

auto LockManager::LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool {
  // 检查事务的隔离级别和当前状态，以决定是否允许加锁
  if (txn->GetIsolationLevel() == IsolationLevel::READ_UNCOMMITTED) {
    // READ_UNCOMMITTED 隔离级别，不能请求共享锁或意向共享锁。
    if (lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_SHARED ||
        lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_SHARED_ON_READ_UNCOMMITTED);
    }
    // 如果事务处于收缩阶段，也不能请求排他锁或意向排他锁
    if (txn->GetState() == TransactionState::SHRINKING &&
        (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE)) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }
  if (txn->GetIsolationLevel() == IsolationLevel::READ_COMMITTED) {
    // READ_UNCOMMITTED 隔离级别，事务不能请求共享锁或意向共享锁
    // 如果事务处于收缩阶段，也不能请求排他锁或意向排他锁
    if (txn->GetState() == TransactionState::SHRINKING && lock_mode != LockMode::INTENTION_SHARED &&
        lock_mode != LockMode::SHARED) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }
  if (txn->GetIsolationLevel() == IsolationLevel::REPEATABLE_READ) {
    // REPEATABLE_READ 隔离级别，事务必须请求所有锁
    // 如果事务处于收缩阶段，不能请求任何锁
    if (txn->GetState() == TransactionState::SHRINKING) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }
  table_lock_map_latch_.lock();
  // 获取与给定表 OID 关联的锁请求队列
  if (table_lock_map_.find(oid) == table_lock_map_.end()) {
    table_lock_map_.emplace(oid, std::make_shared<LockRequestQueue>());
  }
  auto lock_request_queue = table_lock_map_.find(oid)->second;
  lock_request_queue->latch_.lock();
  table_lock_map_latch_.unlock();

  // 检查事务是否已经有锁请求，若有则进行锁升级
  for (auto request : lock_request_queue->request_queue_) {  // NOLINT
    if (request->txn_id_ == txn->GetTransactionId()) {
      // 如果事务已经有锁请求且锁模式相同，则直接返回 true
      if (request->lock_mode_ == lock_mode) {
        lock_request_queue->latch_.unlock();
        return true;
      }

      // 如果有其他事务正在升级锁，中止事务
      if (lock_request_queue->upgrading_ != INVALID_TXN_ID) {
        lock_request_queue->latch_.unlock();
        txn->SetState(TransactionState::ABORTED);
        throw TransactionAbortException(txn->GetTransactionId(), AbortReason::UPGRADE_CONFLICT);
      }

      // 检查当前锁模式和目标锁模式的组合是否允许升级
      if (!(request->lock_mode_ == LockMode::INTENTION_SHARED &&
            (lock_mode == LockMode::SHARED || lock_mode == LockMode::EXCLUSIVE ||
             lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::SHARED &&
            (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::INTENTION_EXCLUSIVE &&
            (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::SHARED_INTENTION_EXCLUSIVE && (lock_mode == LockMode::EXCLUSIVE))) {
        // 如果锁升级不合法，解锁请求队列的互斥锁，中止事务
        lock_request_queue->latch_.unlock();
        txn->SetState(TransactionState::ABORTED);
        throw TransactionAbortException(txn->GetTransactionId(), AbortReason::INCOMPATIBLE_UPGRADE);
      }

      // 锁升级合法，从请求队列中移除当前锁请求，并更新事务的锁集合
      lock_request_queue->request_queue_.remove(request);
      InsertOrDeleteTableLockSet(txn, request, false);

      // 创建一个新的锁请求对象，表示升级后的锁请求
      auto upgrade_lock_request = std::make_shared<LockRequest>(txn->GetTransactionId(), lock_mode, oid);

      // 在请求队列中找到第一个未授予锁的位置，并插入新的升级请求
      std::list<std::shared_ptr<LockRequest>>::iterator lr_iter;
      for (lr_iter = lock_request_queue->request_queue_.begin(); lr_iter != lock_request_queue->request_queue_.end();
           lr_iter++) {
        if (!(*lr_iter)->granted_) {
          break;
        }
      }
      lock_request_queue->request_queue_.insert(lr_iter, upgrade_lock_request);
      lock_request_queue->upgrading_ = txn->GetTransactionId();

      // 使用条件变量等待锁的授予
      std::unique_lock<std::mutex> lock(lock_request_queue->latch_, std::adopt_lock);
      // 如果事务在等待过程中被中止，移除升级请求，并返回 false
      while (!GrantLock(upgrade_lock_request, lock_request_queue)) {
        lock_request_queue->cv_.wait(lock);
        if (txn->GetState() == TransactionState::ABORTED) {
          lock_request_queue->upgrading_ = INVALID_TXN_ID;
          lock_request_queue->request_queue_.remove(upgrade_lock_request);
          lock_request_queue->cv_.notify_all();
          return false;
        }
      }

      // 成功获取锁后，更新事务的锁集合，并通知其他等待的事务
      lock_request_queue->upgrading_ = INVALID_TXN_ID;
      upgrade_lock_request->granted_ = true;
      InsertOrDeleteTableLockSet(txn, upgrade_lock_request, true);

      if (lock_mode != LockMode::EXCLUSIVE) {
        lock_request_queue->cv_.notify_all();
      }
      return true;
    }
  }

  // 若事务没有锁请求，则创建一个新的锁请求对象，并将其插入请求队列
  auto lock_request = std::make_shared<LockRequest>(txn->GetTransactionId(), lock_mode, oid);
  lock_request_queue->request_queue_.push_back(lock_request);

  // 锁定请求队列的互斥锁，并进入一个循环，等待锁的授予
  std::unique_lock<std::mutex> lock(lock_request_queue->latch_, std::adopt_lock);
  while (!GrantLock(lock_request, lock_request_queue)) {
    // 调用 GrantLock 方法尝试授予锁。如果锁未被授予，等待条件变量 cv_
    lock_request_queue->cv_.wait(lock);
    // 如果事务在等待过程中被中止，从请求队列中移除锁请求，并通知所有等待的事务，然后返回 false
    if (txn->GetState() == TransactionState::ABORTED) {
      lock_request_queue->request_queue_.remove(lock_request);
      lock_request_queue->cv_.notify_all();
      return false;
    }
  }

  // 如果锁请求被成功授予，代码会更新锁请求的状态，并将其标记为已授予
  lock_request->granted_ = true;
  // 将锁请求添加到事务的锁集合中
  InsertOrDeleteTableLockSet(txn, lock_request, true);

  // 如果锁模式不是排他锁，通知所有等待的事务
  if (lock_mode != LockMode::EXCLUSIVE) {
    lock_request_queue->cv_.notify_all();
  }

  return true;
}

auto LockManager::UnlockTable(Transaction *txn, const table_oid_t &oid) -> bool {
  table_lock_map_latch_.lock();
  // 锁定全局锁映射 table_lock_map_latch_，并检查是否存在与给定表 OID 关联的锁请求队列
  if (table_lock_map_.find(oid) == table_lock_map_.end()) {
    // 如果锁请求队列不存在，解锁全局锁映射，中止事务
    table_lock_map_latch_.unlock();
    txn->SetState(TransactionState::ABORTED);
    throw bustub::TransactionAbortException(txn->GetTransactionId(), AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD);
  }

  // 检查事务是否持有表中行的锁
  auto s_row_lock_set = txn->GetSharedRowLockSet();
  auto x_row_lock_set = txn->GetExclusiveRowLockSet();

  // 如果事务持有行锁，代码会解锁全局锁映射，中止事务
  if (!(s_row_lock_set->find(oid) == s_row_lock_set->end() || s_row_lock_set->at(oid).empty()) ||
      !(x_row_lock_set->find(oid) == x_row_lock_set->end() || x_row_lock_set->at(oid).empty())) {
    table_lock_map_latch_.unlock();
    txn->SetState(TransactionState::ABORTED);
    throw bustub::TransactionAbortException(txn->GetTransactionId(), AbortReason::TABLE_UNLOCKED_BEFORE_UNLOCKING_ROWS);
  }

  // 获取锁请求队列并锁定它
  auto lock_request_queue = table_lock_map_[oid];

  lock_request_queue->latch_.lock();
  table_lock_map_latch_.unlock();

  // 检查事务是否持有表的锁
  for (auto lock_request : lock_request_queue->request_queue_) {  // NOLINT
    if (lock_request->txn_id_ == txn->GetTransactionId() && lock_request->granted_) {
      // 如果事务持有锁，则从请求队列中移除锁请求，通知所有等待的事务，并解锁请求队列
      lock_request_queue->request_queue_.remove(lock_request);

      lock_request_queue->cv_.notify_all();
      lock_request_queue->latch_.unlock();

      // 根据事务的隔离级别和当前状态，决定是否将事务状态更新为收缩阶段
      if ((txn->GetIsolationLevel() == IsolationLevel::REPEATABLE_READ &&
           (lock_request->lock_mode_ == LockMode::SHARED || lock_request->lock_mode_ == LockMode::EXCLUSIVE)) ||
          (txn->GetIsolationLevel() == IsolationLevel::READ_COMMITTED &&
           lock_request->lock_mode_ == LockMode::EXCLUSIVE) ||
          (txn->GetIsolationLevel() == IsolationLevel::READ_UNCOMMITTED &&
           lock_request->lock_mode_ == LockMode::EXCLUSIVE)) {
        // 进一步检查事务的当前状态，如果事务不是已提交或已中止，则将其状态更新为收缩阶段
        if (txn->GetState() != TransactionState::COMMITTED && txn->GetState() != TransactionState::ABORTED) {
          txn->SetState(TransactionState::SHRINKING);
        }
      }
      // 更新事务的锁集合，并返回 true
      InsertOrDeleteTableLockSet(txn, lock_request, false);
      return true;
    }
  }

  // 如果事务没有持有锁，代码会解锁请求队列，中止事务，并抛出异常
  lock_request_queue->latch_.unlock();
  txn->SetState(TransactionState::ABORTED);
  throw bustub::TransactionAbortException(txn->GetTransactionId(), AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD);
}

auto LockManager::LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const RID &rid) -> bool {
  // 检查锁模式是否为意向锁，如果是，则中止事务并抛出异常
  if (lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::INTENTION_SHARED ||
      lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE) {
    txn->SetState(TransactionState::ABORTED);
    throw TransactionAbortException(txn->GetTransactionId(), AbortReason::ATTEMPTED_INTENTION_LOCK_ON_ROW);
  }
  // 根据事务的隔离级别，检查是否允许加锁
  if (txn->GetIsolationLevel() == IsolationLevel::READ_UNCOMMITTED) {
    if (lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_SHARED ||
        lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_SHARED_ON_READ_UNCOMMITTED);
    }
    if (txn->GetState() == TransactionState::SHRINKING &&
        (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE)) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }
  // 类似地，对于 READ_COMMITTED 和 REPEATABLE_READ 隔离级别，也进行了相应的检查
  if (txn->GetIsolationLevel() == IsolationLevel::READ_COMMITTED) {
    if (txn->GetState() == TransactionState::SHRINKING && lock_mode != LockMode::INTENTION_SHARED &&
        lock_mode != LockMode::SHARED) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }
  if (txn->GetIsolationLevel() == IsolationLevel::REPEATABLE_READ) {
    if (txn->GetState() == TransactionState::SHRINKING) {
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::LOCK_ON_SHRINKING);
    }
  }

  // 如果请求的是排他锁，代码会检查事务是否持有表的排他锁或意向排他锁
  if (lock_mode == LockMode::EXCLUSIVE) {
    if (!txn->IsTableExclusiveLocked(oid) && !txn->IsTableIntentionExclusiveLocked(oid) &&
        !txn->IsTableSharedIntentionExclusiveLocked(oid)) {
      // 如果事务没有表的排他锁或意向排他锁，则中止事务并抛出异常
      txn->SetState(TransactionState::ABORTED);
      throw TransactionAbortException(txn->GetTransactionId(), AbortReason::TABLE_LOCK_NOT_PRESENT);
    }
  }

  // 获取与给定行 RID 关联的锁请求队列
  row_lock_map_latch_.lock();
  // 如果锁请求队列不存在，创建一个新的锁请求队列
  if (row_lock_map_.find(rid) == row_lock_map_.end()) {
    row_lock_map_.emplace(rid, std::make_shared<LockRequestQueue>());
  }
  auto lock_request_queue = row_lock_map_.find(rid)->second;
  lock_request_queue->latch_.lock();
  row_lock_map_latch_.unlock();

  // 检查事务是否已经有锁请求，若有则进行锁升级
  for (auto request : lock_request_queue->request_queue_) {  // NOLINT
    // 如果事务已经有锁请求且锁模式相同，则直接返回 true
    if (request->txn_id_ == txn->GetTransactionId()) {
      if (request->lock_mode_ == lock_mode) {
        lock_request_queue->latch_.unlock();
        return true;
      }

      // 如果有其他事务正在升级锁，中止事务
      if (lock_request_queue->upgrading_ != INVALID_TXN_ID) {
        lock_request_queue->latch_.unlock();
        txn->SetState(TransactionState::ABORTED);
        throw TransactionAbortException(txn->GetTransactionId(), AbortReason::UPGRADE_CONFLICT);
      }

      // 检查当前锁模式和目标锁模式的组合是否允许升级
      if (!(request->lock_mode_ == LockMode::INTENTION_SHARED &&
            (lock_mode == LockMode::SHARED || lock_mode == LockMode::EXCLUSIVE ||
             lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::SHARED &&
            (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::INTENTION_EXCLUSIVE &&
            (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE)) &&
          !(request->lock_mode_ == LockMode::SHARED_INTENTION_EXCLUSIVE && (lock_mode == LockMode::EXCLUSIVE))) {
        lock_request_queue->latch_.unlock();
        txn->SetState(TransactionState::ABORTED);
        throw TransactionAbortException(txn->GetTransactionId(), AbortReason::INCOMPATIBLE_UPGRADE);
      }

      // 锁升级合法，从请求队列中移除当前锁请求，并更新事务的锁集合
      lock_request_queue->request_queue_.remove(request);
      InsertOrDeleteRowLockSet(txn, request, false);
      auto upgrade_lock_request = std::make_shared<LockRequest>(txn->GetTransactionId(), lock_mode, oid, rid);

      // 在请求队列中找到第一个未授予锁的位置，并插入新的升级请求
      std::list<std::shared_ptr<LockRequest>>::iterator lr_iter;
      for (lr_iter = lock_request_queue->request_queue_.begin(); lr_iter != lock_request_queue->request_queue_.end();
           lr_iter++) {
        if (!(*lr_iter)->granted_) {
          break;
        }
      }
      lock_request_queue->request_queue_.insert(lr_iter, upgrade_lock_request);
      lock_request_queue->upgrading_ = txn->GetTransactionId();

      // 使用条件变量等待锁的授予
      std::unique_lock<std::mutex> lock(lock_request_queue->latch_, std::adopt_lock);
      while (!GrantLock(upgrade_lock_request, lock_request_queue)) {
        lock_request_queue->cv_.wait(lock);
        // 如果事务在等待过程中被中止，移除升级请求，并返回 false
        if (txn->GetState() == TransactionState::ABORTED) {
          lock_request_queue->upgrading_ = INVALID_TXN_ID;
          lock_request_queue->request_queue_.remove(upgrade_lock_request);
          lock_request_queue->cv_.notify_all();
          return false;
        }
      }

      // 成功获取锁后，更新事务的锁集合，并通知其他等待的事务
      lock_request_queue->upgrading_ = INVALID_TXN_ID;
      upgrade_lock_request->granted_ = true;
      InsertOrDeleteRowLockSet(txn, upgrade_lock_request, true);

      // 如果锁模式不是排他锁，通知所有等待的事务
      if (lock_mode != LockMode::EXCLUSIVE) {
        lock_request_queue->cv_.notify_all();
      }
      return true;
    }
  }

  // 若事务没有锁请求，则创建一个新的锁请求对象，并将其插入请求队列
  auto lock_request = std::make_shared<LockRequest>(txn->GetTransactionId(), lock_mode, oid, rid);
  lock_request_queue->request_queue_.push_back(lock_request);

  // 锁定请求队列的互斥锁，并进入一个循环，等待锁的授予
  std::unique_lock<std::mutex> lock(lock_request_queue->latch_, std::adopt_lock);
  while (!GrantLock(lock_request, lock_request_queue)) {
    lock_request_queue->cv_.wait(lock);
    // 如果事务在等待过程中被中止，从请求队列中移除锁请求，并通知所有等待的事务，然后返回 false
    if (txn->GetState() == TransactionState::ABORTED) {
      lock_request_queue->request_queue_.remove(lock_request);
      lock_request_queue->cv_.notify_all();
      return false;
    }
  }

  // 如果锁请求被成功授予，代码会更新锁请求的状态，并将其标记为已授予
  lock_request->granted_ = true;
  InsertOrDeleteRowLockSet(txn, lock_request, true);

  // 如果锁模式不是排他锁，通知所有等待的事务
  if (lock_mode != LockMode::EXCLUSIVE) {
    lock_request_queue->cv_.notify_all();
  }

  return true;
}

auto LockManager::UnlockRow(Transaction *txn, const table_oid_t &oid, const RID &rid) -> bool {
  row_lock_map_latch_.lock();
  // 锁定全局行锁映射 row_lock_map_latch_，并检查是否存在与给定行 RID 关联的锁请求队列
  if (row_lock_map_.find(rid) == row_lock_map_.end()) {
    // 如果锁请求队列不存在，解锁全局行锁映射，中止事务
    row_lock_map_latch_.unlock();
    txn->SetState(TransactionState::ABORTED);
    throw bustub::TransactionAbortException(txn->GetTransactionId(), AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD);
  }

  // 获取锁请求队列并锁定
  auto lock_request_queue = row_lock_map_[rid];

  lock_request_queue->latch_.lock();
  row_lock_map_latch_.unlock();

  // 检查事务是否持有行的锁
  for (auto lock_request : lock_request_queue->request_queue_) {  // NOLINT
    if (lock_request->txn_id_ == txn->GetTransactionId() && lock_request->granted_) {
      // 如果事务持有锁，从请求队列中移除锁请求，通知所有等待的事务，并解锁请求队列
      lock_request_queue->request_queue_.remove(lock_request);

      lock_request_queue->cv_.notify_all();
      lock_request_queue->latch_.unlock();

      // 根据事务的隔离级别和当前状态，决定是否将事务状态更新为收缩阶段
      if ((txn->GetIsolationLevel() == IsolationLevel::REPEATABLE_READ &&
           (lock_request->lock_mode_ == LockMode::SHARED || lock_request->lock_mode_ == LockMode::EXCLUSIVE)) ||
          (txn->GetIsolationLevel() == IsolationLevel::READ_COMMITTED &&
           lock_request->lock_mode_ == LockMode::EXCLUSIVE) ||
          (txn->GetIsolationLevel() == IsolationLevel::READ_UNCOMMITTED &&
           lock_request->lock_mode_ == LockMode::EXCLUSIVE)) {
        // 如果事务的隔离级别和锁模式满足特定条件
        // 并且事务的状态不是已提交或已中止，则将事务状态更新为收缩阶段
        if (txn->GetState() != TransactionState::COMMITTED && txn->GetState() != TransactionState::ABORTED) {
          txn->SetState(TransactionState::SHRINKING);
        }
      }
      // 更新事务的锁集合，并返回 true 表示解锁成功
      InsertOrDeleteRowLockSet(txn, lock_request, false);
      return true;
    }
  }

  // 如果事务没有持有锁，解锁请求队列，中止事务，并抛出异常
  lock_request_queue->latch_.unlock();
  txn->SetState(TransactionState::ABORTED);
  throw bustub::TransactionAbortException(txn->GetTransactionId(), AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD);
}

void LockManager::AddEdge(txn_id_t t1, txn_id_t t2) {
  txn_set_.insert(t1);
  txn_set_.insert(t2);
  waits_for_[t1].push_back(t2);
}

void LockManager::RemoveEdge(txn_id_t t1, txn_id_t t2) {
  auto iter = std::find(waits_for_[t1].begin(), waits_for_[t1].end(), t2);
  if (iter != waits_for_[t1].end()) {
    waits_for_[t1].erase(iter);
  }
}

auto LockManager::HasCycle(txn_id_t *txn_id) -> bool {
  // 遍历所有事务，依次DFS，检查是否存在环
  for (auto const &start_txn_id : txn_set_) {
    if (Dfs(start_txn_id)) {
      // 若存在环，返回环中的最大事务 ID
      *txn_id = *active_set_.begin();
      for (auto const &active_txn_id : active_set_) {
        *txn_id = std::max(*txn_id, active_txn_id);
      }
      active_set_.clear();
      return true;
    }
    // 不存在环，清空活跃集合
    active_set_.clear();
  }
  return false;
}

auto LockManager::DeleteNode(txn_id_t txn_id) -> void {
  // 删除当前事务的等待边
  waits_for_.erase(txn_id);

  // 从所有事务的等待边中删除当前事务
  for (auto a_txn_id : txn_set_) {
    if (a_txn_id != txn_id) {
      RemoveEdge(a_txn_id, txn_id);
    }
  }
}

auto LockManager::GetEdgeList() -> std::vector<std::pair<txn_id_t, txn_id_t>> {
  std::vector<std::pair<txn_id_t, txn_id_t>> result;
  // 遍历所有事务，获取所有等待边
  for (auto const &pair : waits_for_) {
    // t1->t2表示t1等待t2
    auto t1 = pair.first;
    for (auto const &t2 : pair.second) {
      result.emplace_back(t1, t2);
    }
  }
  return result;
}

void LockManager::RunCycleDetection() {
  // 在一个循环中运行，定期检查是否存在死锁，并在发现死锁时中止相关事务
  while (enable_cycle_detection_) {
    // 每隔一段时间（cycle_detection_interval）进行一次死锁检测
    std::this_thread::sleep_for(cycle_detection_interval);
    {  // TODO(students): detect deadlock
       // 锁定全局表映射和行映射锁，并遍历每个表和行的锁请求队列
      table_lock_map_latch_.lock();
      row_lock_map_latch_.lock();
      for (auto &pair : table_lock_map_) {
        // 将已授予锁的事务 ID 添加到 granted_set 中
        // 并为每个等待锁的事务添加等待边（调用 AddEdge 方法）
        std::unordered_set<txn_id_t> granted_set;
        // 锁定当前表的全局锁
        pair.second->latch_.lock();
        // 遍历当前表的锁请求队列
        for (auto const &lock_request : pair.second->request_queue_) {
          // 如果锁已授予，则将事务 ID 添加到 granted_set 中
          if (lock_request->granted_) {
            granted_set.emplace(lock_request->txn_id_);
          } else {
            // 如果锁未授予，则为等待锁的事务添加等待边
            for (auto txn_id : granted_set) {
              // 事务ID到表ID的映射
              map_txn_oid_.emplace(lock_request->txn_id_, lock_request->oid_);
              // 添加从所有已授予锁的事务到当前事务的等待边
              AddEdge(lock_request->txn_id_, txn_id);
            }
          }
        }
        // 解锁当前表的全局锁
        pair.second->latch_.unlock();
      }

      // 遍历行锁映射
      for (auto &pair : row_lock_map_) {
        std::unordered_set<txn_id_t> granted_set;
        pair.second->latch_.lock();
        // 遍历当前行的锁请求队列
        for (auto const &lock_request : pair.second->request_queue_) {
          // 如果锁已授予，则将事务 ID 添加到 granted_set 中
          if (lock_request->granted_) {
            granted_set.emplace(lock_request->txn_id_);
          } else {
            // 如果锁未授予，则为等待锁的事务添加等待边
            for (auto txn_id : granted_set) {
              // 事务ID到行ID的映射
              map_txn_rid_.emplace(lock_request->txn_id_, lock_request->rid_);
              // 添加从所有已授予锁的事务到当前事务的等待边
              AddEdge(lock_request->txn_id_, txn_id);
            }
          }
        }
        // 解锁当前行的全局锁
        pair.second->latch_.unlock();
      }

      // 解锁全局表映射锁和行映射锁
      row_lock_map_latch_.unlock();
      table_lock_map_latch_.unlock();

      txn_id_t txn_id;
      // 调用 HasCycle 方法检查是否存在死锁
      while (HasCycle(&txn_id)) {
        // 如果存在死锁，中止死锁中的ID最大的事务
        Transaction *txn = TransactionManager::GetTransaction(txn_id);
        txn->SetState(TransactionState::ABORTED);
        DeleteNode(txn_id);

        // 通知当前表上所有等待当前事务的事务
        if (map_txn_oid_.count(txn_id) > 0) {
          // 先获取表锁
          table_lock_map_[map_txn_oid_[txn_id]]->latch_.lock();
          table_lock_map_[map_txn_oid_[txn_id]]->cv_.notify_all();
          table_lock_map_[map_txn_oid_[txn_id]]->latch_.unlock();
        }

        // 通知当前行上所有等待当前事务的事务
        if (map_txn_rid_.count(txn_id) > 0) {
          // 先获取行锁
          row_lock_map_[map_txn_rid_[txn_id]]->latch_.lock();
          row_lock_map_[map_txn_rid_[txn_id]]->cv_.notify_all();
          row_lock_map_[map_txn_rid_[txn_id]]->latch_.unlock();
        }
      }

      waits_for_.clear();
      safe_set_.clear();
      txn_set_.clear();
      map_txn_oid_.clear();
      map_txn_rid_.clear();
    }
  }
}

// 决定是否可以授予一个新的锁请求
// 两个参数：一个指向 LockRequest 对象的共享指针和一个指向 LockRequestQueue 对象的共享指针
auto LockManager::GrantLock(const std::shared_ptr<LockRequest> &lock_request,
                            const std::shared_ptr<LockRequestQueue> &lock_request_queue) -> bool {
  // 遍历 lock_request_queue 中的所有锁请求（request_queue_）
  for (auto &lr : lock_request_queue->request_queue_) {
    // 对于每个锁请求（lr），如果该请求已经被授予（granted_ 为真）
    // 根据当前锁请求的锁模式（lock_mode_）进行不同的处理
    if (lr->granted_) {
      switch (lock_request->lock_mode_) {
        case LockMode::SHARED:
          // 如果当前锁请求的模式是共享锁，并且队列中的某个锁请求的模式是
          // 意向独占（INTENTION_EXCLUSIVE）、共享意向独占（SHARED_INTENTION_EXCLUSIVE）或独占（EXCLUSIVE）
          // 则返回 false，表示不能授予锁
          if (lr->lock_mode_ == LockMode::INTENTION_EXCLUSIVE ||
              lr->lock_mode_ == LockMode::SHARED_INTENTION_EXCLUSIVE || lr->lock_mode_ == LockMode::EXCLUSIVE) {
            return false;
          }
          break;
        case LockMode::EXCLUSIVE:
          // 如果当前锁请求的模式是独占锁，则直接返回 false，独占锁不允许与其他任何锁共存
          return false;
          break;
        case LockMode::INTENTION_SHARED:
          // 如果当前锁请求的模式是共享意向锁，并且队列中的某个锁请求的模式是独占（EXCLUSIVE）
          // 则返回 false，表示不能授予锁
          if (lr->lock_mode_ == LockMode::EXCLUSIVE) {
            return false;
          }
          break;
        case LockMode::INTENTION_EXCLUSIVE:
          // 如果当前锁请求的模式是意向独占锁，并且队列中的某个锁请求的模式是
          // 共享（SHARED）或共享意向独占（SHARED_INTENTION_EXCLUSIVE）或独占（EXCLUSIVE）
          if (lr->lock_mode_ == LockMode::SHARED || lr->lock_mode_ == LockMode::SHARED_INTENTION_EXCLUSIVE ||
              lr->lock_mode_ == LockMode::EXCLUSIVE) {
            return false;
          }
          break;
        case LockMode::SHARED_INTENTION_EXCLUSIVE:
          // 如果当前锁请求的模式是共享意向独占锁，并且队列中的某个锁请求的模式不是意向共享锁（INTENTION_SHARED）
          // 则返回 false，表示不能授予锁
          if (lr->lock_mode_ != LockMode::INTENTION_SHARED) {
            return false;
          }
          break;
      }
    } else if (lock_request.get() != lr.get()) {
      // 如果某个锁请求没有被授予（granted_ 为假），并且当前锁请求与该锁请求不同（通过指针比较）
      // 则返回 false
      return false;
    } else {
      // 如果某个锁请求没有被授予（granted_ 为假），且当前锁请求与该锁请求相同，则返回 true
      return true;
    }
  }
  // 不在请求队列中，返回 false
  return false;
}

// 用于在事务的锁集合中插入或删除表锁
void LockManager::InsertOrDeleteTableLockSet(Transaction *txn, const std::shared_ptr<LockRequest> &lock_request,
                                             bool insert) {
  switch (lock_request->lock_mode_) {
    case LockMode::SHARED:
      // 如果锁模式是共享锁，并且 insert 为真，则将锁请求的对象 ID（oid_）插入到事务的共享表锁集合中；
      // 否则，从共享表锁集合中删除该对象 ID
      if (insert) {
        txn->GetSharedTableLockSet()->insert(lock_request->oid_);
      } else {
        txn->GetSharedTableLockSet()->erase(lock_request->oid_);
      }
      break;
    case LockMode::EXCLUSIVE:
      // 如果锁模式是独占锁，并且 insert 为真，则将锁请求的对象 ID 插入到事务的独占表锁集合中；
      // 否则，从独占表锁集合中删除该对象 ID
      if (insert) {
        txn->GetExclusiveTableLockSet()->insert(lock_request->oid_);
      } else {
        txn->GetExclusiveTableLockSet()->erase(lock_request->oid_);
      }
      break;
    case LockMode::INTENTION_SHARED:
      // 如果锁模式是意向共享锁，并且 insert 为真，则将锁请求的对象 ID 插入到事务的意向共享表锁集合中；
      // 否则，从意向共享表锁集合中删除该对象 ID
      if (insert) {
        txn->GetIntentionSharedTableLockSet()->insert(lock_request->oid_);
      } else {
        txn->GetIntentionSharedTableLockSet()->erase(lock_request->oid_);
      }
      break;
    case LockMode::INTENTION_EXCLUSIVE:
      // 如果锁模式是意向独占锁，并且 insert 为真，则将锁请求的对象 ID 插入到事务的意向独占表锁集合中；
      // 否则，从意向独占表锁集合中删除该对象 ID
      if (insert) {
        txn->GetIntentionExclusiveTableLockSet()->insert(lock_request->oid_);
      } else {
        txn->GetIntentionExclusiveTableLockSet()->erase(lock_request->oid_);
      }
      break;
    case LockMode::SHARED_INTENTION_EXCLUSIVE:
      // 共享意向独占锁
      if (insert) {
        txn->GetSharedIntentionExclusiveTableLockSet()->insert(lock_request->oid_);
      } else {
        txn->GetSharedIntentionExclusiveTableLockSet()->erase(lock_request->oid_);
      }
      break;
  }
}

// 用于在事务的锁集合中插入或删除行锁
void LockManager::InsertOrDeleteRowLockSet(Transaction *txn, const std::shared_ptr<LockRequest> &lock_request,
                                           bool insert) {
  auto s_row_lock_set = txn->GetSharedRowLockSet();
  auto x_row_lock_set = txn->GetExclusiveRowLockSet();
  switch (lock_request->lock_mode_) {
    case LockMode::SHARED:
      if (insert) {
        InsertRowLockSet(s_row_lock_set, lock_request->oid_, lock_request->rid_);
      } else {
        DeleteRowLockSet(s_row_lock_set, lock_request->oid_, lock_request->rid_);
      }
      break;
    case LockMode::EXCLUSIVE:
      if (insert) {
        InsertRowLockSet(x_row_lock_set, lock_request->oid_, lock_request->rid_);
      } else {
        DeleteRowLockSet(x_row_lock_set, lock_request->oid_, lock_request->rid_);
      }
      break;
    // 行锁不存在意向锁
    case LockMode::INTENTION_SHARED:
    case LockMode::INTENTION_EXCLUSIVE:
    case LockMode::SHARED_INTENTION_EXCLUSIVE:
      break;
  }
}

}  // namespace bustub
