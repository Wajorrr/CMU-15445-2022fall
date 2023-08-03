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
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) -> void {
  array_[index].second = value;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIdx(ValueType value) const -> int {
  for (int i = 0; i < this->GetSize(); i++) {
    if (array_[i].second == value) {
      return i;
    }
  }
  return 0;
}

// 给定key，返回其所对应的位置
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Search(const KeyType &key, const KeyComparator &comparator) const -> int {
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
  return l;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SplitCopy(BPlusTreeInternalPage *node, MappingType child_item,
                                               KeyComparator comparator, BufferPoolManager *buffer_pool_manager_) {
  // 移动一半kv对到新的node中
  int move_num =
      (node->GetSize() + 2) / 2;  // 要分出去的kv对数量 //这里还有一个节点没插入(先插入再分裂的话会越界)，要另外+1
  // 由于是flexible数组，因此应该从尾部分出一半kv对，即分出一个新的右边internal node出来
  // 由于internal node的第一个kv对中的key默认被视为不使用，因此直接移动一半kv对即可，不需要对第一个key进行删除
  //  这样就可以直接通过减少原node的size来达到删除效果

  int start_idx = node->GetSize() + 1 - move_num;
  int child_idx = node->Search(child_item.first, comparator) + 1;
  for (int i = 0; i < move_num; i++) {
    if (start_idx + i < child_idx) {
      array_[i] = node->array_[start_idx + i];
    } else if (start_idx + i == child_idx) {
      array_[i] = child_item;
    } else {
      array_[i] = node->array_[start_idx + i - 1];
    }
    // 注意，这里和leaf node不一样了
    // 将internal node中的kv对移动后，需要遍历每个v(child node)，将每个儿子节点的父节点id更新
    auto child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(array_[i].second)->GetData());
    child_page->SetParentPageId(this->GetPageId());
    buffer_pool_manager_->UnpinPage(child_page->GetPageId(),
                                    true);  // 对child page进行了更新，设置dirty标记为true
  }
  node->IncreaseSize(1 - move_num);  // 原node size减少
  if (child_idx < start_idx) {
    node->NewNodeInsert(child_item.first, child_item.second, comparator);
  }
  IncreaseSize(move_num);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::LinkToNewRoot(ValueType node_id, KeyType key, ValueType new_node_id) {
  SetValueAt(0, node_id);
  SetKeyAt(1, key);
  SetValueAt(1, new_node_id);
  SetSize(2);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::NewNodeInsert(KeyType key, ValueType new_node_id,
                                                   const KeyComparator &comparator) {
  int idx = Search(key, comparator);
  for (int i = GetSize() - 1; i > idx; i--) {
    array_[i + 1] = array_[i];
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
    auto *child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[i].second)->GetData());
    child_page->SetParentPageId(node->GetPageId());
    buffer_pool_manager->UnpinPage(array_[i].second, true);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirst(BPlusTreeInternalPage *node, const KeyType &first_key,
                                               BufferPoolManager *buffer_pool_manager) {
  array_[0].first = first_key;
  node->array_[node->GetSize()] = array_[0];
  auto *child_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[0].second)->GetData());
  child_page->SetParentPageId(node->GetPageId());
  buffer_pool_manager->UnpinPage(array_[0].second, true);
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
  auto *child_page =
      reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(array_[GetSize() - 1].second)->GetData());
  child_page->SetParentPageId(node->GetPageId());
  buffer_pool_manager->UnpinPage(array_[GetSize() - 1].second, true);
}

// valuetype for internalNode should be page_id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;

}  // namespace bustub
