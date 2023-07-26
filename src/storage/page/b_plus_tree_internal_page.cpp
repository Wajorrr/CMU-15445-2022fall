//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

namespace bustub {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetMaxSize(max_size);
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetSize(0);
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  // replace with your own code
  return array_[index].first;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) { array_[index].first = key; }

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType { return array_[index].second; }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIdx(ValueType value) const -> int {
  for (int i = 0; i < this->GetSize(); i++) {
    if (array_[i].second == value) {
      return i;
    }
  }
  return 0;
}

// 给定key，返回其所对应的位置的Value值
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Search(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  // 给定key，返回key所在/应插入的child,即找到第一个大于key的array_[x].first，返回x-1对应的child(实际返回的就是page_id)
  int l = 0;
  int r = GetSize() - 1;  // 初始区间[l,r]=[0,n-1]
  while (l < r) {
    int mid = (l + r + 1) >> 1;
    if (comparator(array_[mid].first, key) <= 0) {  // mid小于等于key，则左端点移到mid
      l = mid;
    } else {  // mid大于key，则右端点移到mid-1
      r = mid - 1;
    }
  }
  return array_[l].second;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SplitCopy(BPlusTreeInternalPage *node, int startidx, int num,
                                               BufferPoolManager *buffer_pool_manager_) {
  for (int i = 0; i < num; i++) {
    array_[i] = node->array_[startidx + i];
    // 注意，这里和leaf node不一样了
    // 将internal node中的kv对移动后，需要遍历每个v(child node)，将每个儿子节点的父节点id更新
    auto child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(array_[i].second));
    child_page->SetParentPageId(this->GetPageId());
    buffer_pool_manager_->UnpinPage(array_[i].second,
                                    true);  // 对child page进行了更新，设置dirty标记为true
  }
}
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::LinkToNewRoot(ValueType node_id, KeyType key, ValueType new_node_id) {
  array_[0].second = node_id;
  array_[1].first = key;
  array_[1].second = new_node_id;
  SetSize(2);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::NewNodeInsert(ValueType node_id, KeyType key, ValueType new_node_id) {
  int idx = ValueIdx(node_id);
  for (int i = GetSize(); i > idx + 1; i--) {
    array_[i] = array_[i - 1];
  }
  array_[idx + 1] = MappingType{key, new_node_id};
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int idx) {
  for (int i = idx; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
  IncreaseSize(-1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAll(BPlusTreeInternalPage *node, const KeyType &first_key,
                                             BufferPoolManager *buffer_pool_manager) {
  int num = GetSize();
  int startidx = node->GetSize();
  array_[0].first = first_key;  // 原本位于第一个的key是无效的，现在要给其赋值为一个分界key(从parent节点获取)
  for (int i = 0; i < num; i++) {
    node->array_[startidx + i] = array_[i];
    // 还要更新所有子节点的parent_page_id
    auto *child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[i].second));
    child_page->SetParentPageId(node->GetPageId());
    buffer_pool_manager->UnpinPage(array_[i].second, true);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirst(BPlusTreeInternalPage *node, const KeyType &first_key,
                                               BufferPoolManager *buffer_pool_manager) {
  array_[0].first = first_key;
  node->array_[node->GetSize()] = array_[0];
  auto *child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[0].second));
  child_page->SetParentPageId(node->GetPageId());
  buffer_pool_manager->UnpinPage(child_page->GetPageId(), true);
  for (int i = 0; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLast(BPlusTreeInternalPage *node, const KeyType &first_key,
                                              BufferPoolManager *buffer_pool_manager) {
  node->array_[0].first = first_key;
  for (int i = 0; i < node->GetSize(); i++) {
    node->array_[i + 1] = node->array_[i];
  }
  node->array_[0] = array_[GetSize() - 1];
  auto *child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[GetSize() - 1].second));
  child_page->SetParentPageId(node->GetPageId());
  buffer_pool_manager->UnpinPage(child_page->GetPageId(), true);
}

// valuetype for internalNode should be page_id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;

}  // namespace bustub
