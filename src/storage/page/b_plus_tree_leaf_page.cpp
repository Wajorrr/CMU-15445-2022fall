//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_leaf_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetMaxSize(max_size);
  SetPageType(IndexPageType::LEAF_PAGE);
  SetNextPageId(INVALID_PAGE_ID);
  SetSize(0);
}

/**
 * Helper methods to set/get next page id
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t { return next_page_id_; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) { next_page_id_ = next_page_id; }

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  // replace with your own code
  return array_[index].first;
}

// 给定key，返回其所对应的位置下标
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  int l = 0;
  int r = GetSize();
  while (l < r) {
    int mid = (l + r) >> 1;
    if (comparator(array_[mid].first, key) >= 0) {
      r = mid;
    } else {
      l = mid + 1;
    }
  }
  return l;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetItem(int index) -> MappingType & { return array_[index]; }

// 给定key，查找是否存在相应的kv对，没找到则返回false，result返回key索引的所有value值
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetValue(const KeyType &key, const KeyComparator &comparator,
                                          std::vector<ValueType> *result) const -> bool {
  int idx = KeyIndex(key, comparator);
  if (comparator(array_[idx].first, key) != 0) {  // array中找不到给定的key值
    return false;
  }
  if (result == nullptr) {  // array中有给定的key值，但不需要返回value列表
    return true;
  }
  // 返回value列表
  // std::cout << "get value:key=" << key << "\n";
  while (idx < GetSize() && comparator(array_[idx].first, key) == 0) {
    result->push_back(array_[idx++].second);
    // std::cout << array_[idx - 1].second << " ";
  }
  // std::cout << "\n";
  return true;
}

// 向leaf node中插入kv对
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value, const KeyComparator &comparator)
    -> int {
  if (GetSize() == GetMaxSize()) {
    // std::cout << "leaf_page Insert: size=" << GetSize() << ", max_size=" << GetMaxSize() << std::endl;
  }
  int idx = KeyIndex(key, comparator);  // 找到要插入的下标
  // std::cout << "insert idx:" << idx << "\n";
  for (int i = GetSize(); i >= idx; i--) {  // 后面的元素整体后移一位
    array_[i] = array_[i - 1];
  }
  array_[idx] = MappingType{key, value};
  IncreaseSize(1);
  // std::cout << "inserted: key-{" << key << "} value-{" << value << "} getsize-" << GetSize() << "\n";
  return GetSize();
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SplitCopy(BPlusTreeLeafPage *node, int startidx, int num) {
  for (int i = 0; i < num; i++) {
    array_[i] = node->array_[startidx + i];
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveItem(const KeyType &key, KeyComparator &comparator) -> int {
  int idx = KeyIndex(key, comparator);
  if (idx < GetSize() && comparator(array_[idx].first, key) == 0) {
    for (int i = idx; i < GetSize() - 1; i++) {
      array_[i] = array_[i + 1];
    }
  }
  IncreaseSize(-1);
  return GetSize();
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveAll(BPlusTreeLeafPage *node) {
  int num = GetSize();
  int startidx = node->GetSize();
  for (int i = 0; i < num; i++) {
    node->array_[startidx + i] = array_[i];
  }
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveFirst(BPlusTreeLeafPage *node) {
  node->array_[node->GetSize()] = array_[0];
  for (int i = 0; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveLast(BPlusTreeLeafPage *node) {
  for (int i = 0; i < node->GetSize(); i++) {
    node->array_[i + 1] = node->array_[i];
  }
  node->array_[0] = array_[GetSize() - 1];
}

template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
