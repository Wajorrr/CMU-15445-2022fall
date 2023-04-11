//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/logger.h"

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {
  for (size_t i = 0; i < num_frames; i++) {
    list_.emplace_front(Frame{static_cast<int>(i)});
    table_.emplace(i, list_.begin());
    // SetEvictable(i, true);
  }
  first_less_k_ = list_.end();
  first_larger_k_ = list_.end();
}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);

  if (curr_size_ == 0) {
    return false;
  }

  auto iter = list_.end();

  do {
    iter--;
  } while (iter != list_.begin() && !iter->IsEvictable());

  if (iter == list_.begin() && !iter->IsEvictable()) {
    return false;
  }

  *frame_id = iter->GetId();
  if (iter == first_less_k_) {
    first_less_k_++;
  }
  if (iter == first_larger_k_) {
    first_larger_k_++;
  }
  // std::cout << "evict frame:" << *frame_id << "\n";
  iter->Reset();                             // frame被free，重置
  list_.splice(list_.begin(), list_, iter);  // 将其移到链表头
  curr_size_--;                              // 更新当前size
  return true;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  BUSTUB_ASSERT(frame_id >= 0 && static_cast<uint>(frame_id) < replacer_size_, "invalid frame_id!");
  std::scoped_lock<std::mutex> lock(latch_);

  current_timestamp_++;
  auto iter = table_[frame_id];

  // LOG_DEBUG("frame_id:%d,access times:%ld,evictable:%d", frame_id, iter->GetAccessTimes(), iter->IsEvictable());
  // LOG_DEBUG("replacer_size:%ld", curr_size_);

  if (iter->GetAccessTimes() ==
      0) {  // frame没被使用，则为其设置访问次数，访问时间戳，并将其移到访问次数少于k次的frame列表头部
    iter->IncreaseAccessTimes();

    list_.splice(first_less_k_, list_, iter);  // 移动
    if (first_larger_k_ == first_less_k_) {
      first_larger_k_ = iter;
    }
    first_less_k_ = iter;  // 更新访问次数少于k次的frame列表头指针
    // iter->SetEvictable(true);
    // curr_size_++;
  } else if (iter->GetAccessTimes() < k_) {  // frame的访问次数少于k次
    iter->IncreaseAccessTimes();
    if (iter->GetAccessTimes() >= k_) {  // 若访问后其访问次数等于k次，将其移到访问次数大于k次的frame列表头部
      if (iter == first_less_k_) {  // 若访问次数少于k次的frame列表头指针指向当前frame，则将其指向下一个frame
        first_less_k_++;
      }
      list_.splice(first_larger_k_, list_, iter);
      first_larger_k_ = iter;  // 更新访问次数多于k次的frame列表头指针
    }
    // 若访问后其访问次数仍少于k次，不移动

  } else {  // frame的访问次数大于等于k次，将其移到访问次数大于k次的frame列表头部
    iter->IncreaseAccessTimes();
    list_.splice(first_larger_k_, list_, iter);
    first_larger_k_ = iter;  // 更新访问次数多于k次的frame列表头指针
  }
  // LOG_DEBUG("frame_id:%d,access times:%ld,evictable:%d", frame_id, iter->GetAccessTimes(), iter->IsEvictable());
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock<std::mutex> lock(latch_);

  // BUSTUB_ASSERT(table_[frame_id]->GetAccessTimes() > 0, "frame is not used!");
  if (table_[frame_id]->GetAccessTimes() == 0) {
    return;
  }

  // std::cout << "set " << frame_id << " " << set_evictable << "\n";
  if (table_[frame_id]->IsEvictable() != set_evictable) {
    curr_size_ += set_evictable ? 1 : -1;
  }

  table_[frame_id]->SetEvictable(set_evictable);
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);

  // BUSTUB_ASSERT(table_[frame_id]->IsEvictable(), "This Frame is non-evictable!");
  // BUSTUB_ASSERT(frame_id >= 0 && static_cast<uint>(frame_id) < replacer_size_, "invalid frame_id!");
  // BUSTUB_ASSERT(table_[frame_id]->GetAccessTimes() > 0, "frame is not used!");
  if (!table_[frame_id]->IsEvictable()) {
    return;
  }

  if (frame_id < 0 || static_cast<uint>(frame_id) >= replacer_size_) {
    return;
  }

  if (curr_size_ == 0) {
    return;
  }

  auto iter = table_[frame_id];
  if (iter == first_larger_k_) {
    first_larger_k_++;
  } else if (iter == first_less_k_) {
    first_less_k_++;
  }
  // std::cout << "remove frame:" << frame_id << "\n";
  iter->Reset();
  list_.splice(list_.begin(), list_, iter);
  curr_size_--;
}

auto LRUKReplacer::Size() -> size_t { return curr_size_; }

}  // namespace bustub
