/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(B_PLUS_TREE_LEAF_PAGE_TYPE *left_most_leaf, int index, BufferPoolManager *bpm)
    : cur_leaf_page_(left_most_leaf), index_(index), buffer_pool_manager_(bpm) {}

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() {
  if (cur_leaf_page_ != nullptr) {
    buffer_pool_manager_->UnpinPage(cur_leaf_page_->GetPageId(), false);
  }
};  // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool {
  return cur_leaf_page_->GetNextPageId() == INVALID_PAGE_ID && index_ == cur_leaf_page_->GetSize();
}

INDEX_TEMPLATE_ARGUMENTS auto INDEXITERATOR_TYPE::operator*() -> const MappingType & {
  return cur_leaf_page_->GetItem(index_);
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  index_++;
  if (index_ == cur_leaf_page_->GetSize() &&
      cur_leaf_page_->GetNextPageId() != INVALID_PAGE_ID) {  // 到达一个page的末尾了
    page_id_t next_page_id = cur_leaf_page_->GetNextPageId();
    buffer_pool_manager_->UnpinPage(cur_leaf_page_->GetPageId(), false);
    cur_leaf_page_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(buffer_pool_manager_->FetchPage(next_page_id));
    index_ = 0;
  }
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
