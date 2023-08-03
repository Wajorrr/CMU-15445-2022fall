//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

/*
 * Helper methods to get/set page type
 * Page type enum class is defined in b_plus_tree_page.h
 */
auto BPlusTreePage::IsLeafPage() const -> bool { return page_type_ == IndexPageType::LEAF_PAGE; }
auto BPlusTreePage::IsRootPage() const -> bool { return parent_page_id_ == INVALID_PAGE_ID; }
void BPlusTreePage::SetPageType(IndexPageType page_type) { page_type_ = page_type; }

/*
 * Helper methods to get/set size (number of key/value pairs stored in that
 * page)
 */
auto BPlusTreePage::GetSize() const -> int { return size_; }
void BPlusTreePage::SetSize(int size) { size_ = size; }
void BPlusTreePage::IncreaseSize(int amount) { size_ += amount; }

/*
 * Helper methods to get/set max size (capacity) of the page
 */
auto BPlusTreePage::GetMaxSize() const -> int { return max_size_; }
void BPlusTreePage::SetMaxSize(int size) { max_size_ = size; }

/*
 * Helper method to get min page size
 * Generally, min page size == max page size / 2
 */
auto BPlusTreePage::GetMinSize() const -> int {
  if (IsLeafPage()) {
    return max_size_ >> 1;
  }
  return (max_size_ + 1) >> 1;  // internal节点的minsize
}

/*
 * Helper methods to get/set parent page id
 */
auto BPlusTreePage::GetParentPageId() const -> page_id_t { return parent_page_id_; }
void BPlusTreePage::SetParentPageId(page_id_t parent_page_id) { parent_page_id_ = parent_page_id; }

/*
 * Helper methods to get/set self page id
 */
auto BPlusTreePage::GetPageId() const -> page_id_t { return page_id_; }
void BPlusTreePage::SetPageId(page_id_t page_id) { page_id_ = page_id; }

/*
 * Helper methods to set lsn
 */
void BPlusTreePage::SetLSN(lsn_t lsn) { lsn_ = lsn; }

// void BPlusTreePage::RLock() {
//   std::unique_lock<std::mutex> latch(mutex_);
//   while (writer_entered_ || reader_count_ == MAX_READER_COUNT) {
//     reader_.wait(latch);
//   }
//   reader_count_++;
// }
// void BPlusTreePage::RUnlock() {
//   std::unique_lock<std::mutex> latch(mutex_);
//   reader_count_--;
//   if (writer_entered_) {
//     if (reader_count_ == 0) {
//       writer_.notify_one();
//     }
//   } else if (reader_count_ == MAX_READER_COUNT - 1) {
//     reader_.notify_one();
//   }
// }
// void BPlusTreePage::WLock() {
//   std::cout << "page " << GetPageId() << " try lock\n";
//   std::unique_lock<std::mutex> latch(mutex_);
//   while (writer_entered_) {
//     reader_.wait(latch);
//   }
//   writer_entered_ = true;
//   if (reader_count_ > 0) {
//     writer_.wait(latch);
//   }
//   std::cout << "page " << GetPageId() << " locked\n";
// }
// void BPlusTreePage::WUnlock() {
//   std::cout << "page " << GetPageId() << " try unlock\n";
//   std::unique_lock<std::mutex> latch(mutex_);
//   writer_entered_ = false;
//   reader_.notify_all();
//   std::cout << "page " << GetPageId() << " unlocked\n";
// }

}  // namespace bustub
