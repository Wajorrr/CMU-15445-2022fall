#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) -> bool {
  if (IsEmpty()) {
    return false;
  }
  auto *page = reinterpret_cast<LeafPage *>(FindLeafPage(key));
  page->GetValue(key, comparator_, result);
  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
  // std::cout << "result:";
  //  for (auto i : *result) {
  //    std::cout << i << " ";
  //  }
  //  std::cout << "\n";
  return !result->empty();
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafPage(const KeyType &key) const -> BPlusTreePage * {
  // 给定key，返回key所在/要插入的leaf page
  if (IsEmpty()) {
    throw std::runtime_error("error!root page id is NVALID_PAGE_ID!");
  }
  Page *root_page =
      buffer_pool_manager_->FetchPage(root_page_id_);  // 获取根结点page，此函数调用后会将获取到的page的pin_count++
  auto *page = reinterpret_cast<BPlusTreePage *>(root_page);
  while (!page->IsLeafPage()) {  // 只要不是叶子节点，则循环向下找
    auto *internal_page = reinterpret_cast<InternalPage *>(page);
    page_id_t next_id = internal_page->Search(key, comparator_);
    page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(next_id));
    // 这里记得将之前获取到的page进行unpin
    buffer_pool_manager_->UnpinPage(internal_page->GetPageId(), false);  // UnpinPgImp(page_id_t page_id, bool is_dirty)
  }
  return page;  // 注意这里return时，page仍是pin的状态
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  // std::cout << "insert:" << key << " " << value;
  // std::cout << "IsEmpty:" << IsEmpty() << "\n";
  // 若当前树为空，则创建根节点
  if (IsEmpty()) {
    Page *new_page = buffer_pool_manager_->NewPage(&root_page_id_);  // 从缓存申请一个新page，pin+1
    if (new_page == nullptr) {
      throw "no available frame";
    }
    UpdateRootPageId(true);  // 更新root page id
    auto *leaf_page = reinterpret_cast<LeafPage *>(new_page);
    leaf_page->Init(root_page_id_, INVALID_PAGE_ID,
                    leaf_max_size_);  // 三个参数分别为当前page的page_id、父节点page的parent_id、叶子节点最大容量
    leaf_page->Insert(key, value, comparator_);  // 插入key value对
    // std::cout << "inserted: key-{" << key << "} value-{" << value << "} getsize-" << leaf_page->GetSize() << "\n";
    buffer_pool_manager_->UnpinPage(root_page_id_,
                                    true);  // pin-1，由于向page中插入了kv对，需要设置page的dirty标记为true
    return true;
  }

  auto page = FindLeafPage(key);  // 返回的page是pin的状态
  auto *leaf_page = reinterpret_cast<LeafPage *>(page);

  if (leaf_page->GetValue(key, comparator_)) {  // 已存在key
    buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    return false;
  }
  // key不存在，插入
  leaf_page->Insert(key, value, comparator_);
  // std::cout << "inserted: key-{" << key << "} value-{" << value << "} getsize-" << leaf_page->GetSize()
  //           << "\n maxsize:" << leaf_page->GetMaxSize() << "\n";

  if (leaf_page->GetSize() >= leaf_page->GetMaxSize()) {
    auto *new_leaf_page = SplitNode(leaf_page);
    InsertIntoParent(leaf_page, new_leaf_page->KeyAt(0), new_leaf_page);
    buffer_pool_manager_->UnpinPage(new_leaf_page->GetPageId(), true);
  }
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
  return true;
}

INDEX_TEMPLATE_ARGUMENTS
template <typename T>  // 有必要用这个吗？
auto BPLUSTREE_TYPE::SplitNode(T *node) -> T * {
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);  // 申请new page，获取new_page_id
  if (new_page == nullptr) {
    throw "out of memory";
  }

  T *new_node;

  if (node->IsLeafPage()) {                                        // 若节点为leaf node
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);          // 原node
    auto *new_leaf_node = reinterpret_cast<LeafPage *>(new_page);  // 新node
    new_leaf_node->Init(new_page_id, leaf_node->GetParentPageId(),
                        leaf_max_size_);  // Init(page_id_t page_id, page_id_t parent_id, int max_size)

    // 移动一半kv对到新的node中
    int move_num = (leaf_node->GetSize() + 1) / 2;  // 要分出去的kv对数量
    // 由于是flexible数组，因此应该从尾部分出一半kv对，即分出一个新的右边leaf node出来
    // 这样就可以直接通过减少原node的size来达到删除效果
    int start_pos = leaf_node->GetSize() - move_num;
    new_leaf_node->SplitCopy(leaf_node, start_pos, move_num);
    leaf_node->IncreaseSize(-1 * move_num);  // 原node size减少
    new_leaf_node->IncreaseSize(move_num);

    // 更新双向链表
    new_leaf_node->SetNextPageId(leaf_node->GetNextPageId());
    leaf_node->SetNextPageId(new_leaf_node->GetPageId());
    new_node = reinterpret_cast<T *>(new_leaf_node);
  } else {  // 若节点为internalnode
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *new_internal_node = reinterpret_cast<InternalPage *>(new_page);
    new_internal_node->Init(new_page_id, internal_node->GetParentPageId(),
                            internal_max_size_);  // Init(page_id_t page_id, page_id_t parent_id, int max_size)

    // 移动一半kv对到新的node中
    int move_num = (internal_node->GetSize() + 1) / 2;  // 要分出去的kv对数量
    // 由于是flexible数组，因此应该从尾部分出一半kv对，即分出一个新的右边internal node出来
    // 由于internal node的第一个kv对中的key默认被视为不使用，因此直接移动一半kv对即可，不需要对第一个key进行删除
    //  这样就可以直接通过减少原node的size来达到删除效果
    int start_pos = internal_node->GetSize() - move_num;
    new_internal_node->SplitCopy(internal_node, start_pos, move_num, buffer_pool_manager_);
    internal_node->IncreaseSize(-1 * move_num);  // 原node size减少
    new_internal_node->IncreaseSize(move_num);
    new_node = reinterpret_cast<T *>(new_internal_node);
  }
  return new_node;  // 返回新建的节点指针
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *node, const KeyType &key, BPlusTreePage *new_node) {
  if (node->IsRootPage()) {  // 原节点已经是root节点，则需要新建root节点
    Page *page = buffer_pool_manager_->NewPage(&root_page_id_);  // 直接将root_page_id更新为新申请的page id
    auto *new_root_node = reinterpret_cast<InternalPage *>(page);

    new_root_node->Init(
        root_page_id_, INVALID_PAGE_ID,
        internal_max_size_);  // 三个参数分别为当前page的page_id、父节点page的parent_id、internal节点最大容量
    UpdateRootPageId(false);

    // 初始化新root节点的array_和size
    new_root_node->LinkToNewRoot(node->GetPageId(), key, new_node->GetPageId());

    // 将原节点和新节点均设为新root节点的子节点
    node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);
    buffer_pool_manager_->UnpinPage(root_page_id_, true);  // 对新建root节点进行了修改，dirty标记设置为true
  } else {
    page_id_t parent_page_id = node->GetParentPageId();
    auto *parent_node = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(parent_page_id));

    // 将新节点插入到父节点的array中的相应位置
    parent_node->NewNodeInsert(node->GetPageId(), key, new_node->GetPageId());
    parent_node->IncreaseSize(1);
    new_node->SetParentPageId(parent_page_id);
    // 感觉可以在这里对node和new_node进行unpin，否则到后面进行递归之后再进行，会造成整个更新链上的page全部被占用
    if (parent_node->GetSize() > parent_node->GetMaxSize()) {
      InternalPage *parent_split_node = SplitNode(parent_node);
      // 注意，key取的是split后创建的新节点中idx为0的key，这个key在新节点中本身不会被使用到，但是是原节点和新节点的分界值
      InsertIntoParent(parent_node, parent_split_node->KeyAt(0), parent_split_node);
      buffer_pool_manager_->UnpinPage(parent_split_node->GetPageId(), true);
    }
    buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (IsEmpty()) {
    return;
  }

  auto *leaf_page = reinterpret_cast<LeafPage *>(FindLeafPage(key));  // leaf_page为pinned
  leaf_page->RemoveItem(key, comparator_);

  if (leaf_page->GetSize() < leaf_page->GetMinSize()) {
    CoalesceOrRedistribute(leaf_page);
  }

  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
template <typename T>
void BPLUSTREE_TYPE::CoalesceOrRedistribute(T *node) {
  if (node->IsRootPage()) {  // 根节点的情况单独考虑，这里也是递归更新的终点
    AdjustRoot(node);
    return;
  }
  // 获取父节点以及兄弟节点的指针
  auto *parent_page =
      reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(node->GetParentPageId()));  // parent_page pinned
  int node_idx = parent_page->ValueIdx(node->GetPageId());
  // 默认取左边的兄弟节点来进行合并或调整，若当前节点为第一个节点，则取右边的兄弟节点
  int sibling_idx = node_idx - 1;  // 左边节点的idx
  if (node_idx == 0) {
    sibling_idx = node_idx + 1;  // 右边节点的idx
  }
  int sibling_page_id = parent_page->ValueAt(sibling_idx);
  T *sibling_node = reinterpret_cast<T *>(buffer_pool_manager_->FetchPage(sibling_page_id));  // sibling_node pinned

  // node和其左边节点的size之和不大于max size，则合并，注意internal page的maxsize和leaf page的maxsize是不同的
  if (node->GetSize() + sibling_node->GetSize() <= (node->IsLeafPage() ? node->GetMaxSize() - 1 : node->GetMaxSize())) {
    if (node_idx == 0) {
      // 若取的是node的右边节点进行合并，则将node和sibling互换，保证sibling在左node在右，将右边node节点合并到左边节点
      T *temp = node;
      node = sibling_node;
      sibling_node = temp;
      std::swap(node_idx, sibling_idx);
    }
    Coalesce(sibling_node, node, parent_page, node_idx);  // 将node合并到sibling_node
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(sibling_node->GetPageId(), true);
    return;
  }

  // node和其兄弟节点的size之和超过max size，则从兄弟节点借kv对
  Redistribute(sibling_node, node, parent_page, node_idx);
  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(sibling_node->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    // 由于之后不会再用到这个page，直接将其从内存中删除，不用写回硬盘
    buffer_pool_manager_->UnpinPage(old_root_node->GetPageId(), false);
    buffer_pool_manager_->DeletePage(old_root_node->GetPageId());
    root_page_id_ = INVALID_PAGE_ID;
    UpdateRootPageId(false);
    return;
  }
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    // 删除根节点，把其唯一的子节点作为新的根节点
    auto old_root_page = reinterpret_cast<InternalPage *>(old_root_node);
    root_page_id_ = old_root_page->ValueAt(0);
    old_root_page->IncreaseSize(-1);
    UpdateRootPageId(false);

    auto *new_root_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_));
    new_root_page->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
    // 从内存中删除原根节点对应的page
    buffer_pool_manager_->UnpinPage(old_root_page->GetPageId(), false);
    buffer_pool_manager_->DeletePage(old_root_page->GetPageId());
  }
}

INDEX_TEMPLATE_ARGUMENTS
template <typename T>
void BPLUSTREE_TYPE::Coalesce(T *sibling, T *node, InternalPage *parent_node, int node_idx) {
  if (node->IsLeafPage()) {  // leaf node合并
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *sibling_node = reinterpret_cast<LeafPage *>(sibling);
    leaf_node->MoveAll(sibling_node);
    leaf_node->IncreaseSize(-1 * leaf_node->GetSize());
    sibling_node->IncreaseSize(leaf_node->GetSize());
    sibling_node->SetNextPageId(leaf_node->GetNextPageId());  // 更新next指针
  } else {
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *sibling_node = reinterpret_cast<InternalPage *>(sibling);
    internal_node->MoveAll(sibling_node, parent_node->KeyAt(node_idx),
                           buffer_pool_manager_);  // 用parent node中指向当前node的key代替node中的第一个key,再进行合并
    internal_node->IncreaseSize(-1 * internal_node->GetSize());
    sibling_node->IncreaseSize(internal_node->GetSize());
  }
  buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
  buffer_pool_manager_->DeletePage(node->GetPageId());  // 原node已被合并到左边的节点，将原node对应的page删除
  parent_node->Remove(node_idx);                        // 删除父节点中原node对应的记录
  if (parent_node->GetSize() < parent_node->GetMinSize()) {
    CoalesceOrRedistribute(parent_node);  // 若父节点删除之后也小于minsize，则递归进行调整
  }
}

INDEX_TEMPLATE_ARGUMENTS
template <typename T>
void BPLUSTREE_TYPE::Redistribute(T *sibling_node, T *node, InternalPage *parent_node, int node_idx) {
  if (sibling_node->IsLeafPage()) {  // leaf node
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *leaf_sibling_node = reinterpret_cast<LeafPage *>(sibling_node);
    if (node_idx == 0) {  // node左边没有节点了，即sibling node为右边的节点，将sibling node的第一个kv对移到node尾部
      leaf_sibling_node->MoveFirst(leaf_node);
      parent_node->SetKeyAt(node_idx + 1, leaf_sibling_node->KeyAt(0));
    } else {  // sibling node在node节点的左边，将sibling node的最后一个kv对移到node头部
      leaf_sibling_node->MoveLast(leaf_node);
      parent_node->SetKeyAt(node_idx, leaf_node->KeyAt(0));  // 更新父节点的索引
    }
    leaf_sibling_node->IncreaseSize(-1);
    leaf_node->IncreaseSize(1);
  } else {  // internal node
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *internal_sibling_node = reinterpret_cast<InternalPage *>(sibling_node);
    if (node_idx == 0) {  // sibling node在右
                          // 注意需要将sibling_node将被移动的第一个kv对的key值更新，并将value值对应的child的parent更新
      internal_sibling_node->MoveFirst(internal_node, parent_node->KeyAt(node_idx + 1), buffer_pool_manager_);
      parent_node->SetKeyAt(node_idx + 1, internal_sibling_node->KeyAt(0));
    } else {  // sibling node在左
      // 注意需要将node的第一个kv对的key值更新，并将sibling_node的最后一个value值对应的child的parent更新
      internal_sibling_node->MoveLast(internal_node, parent_node->KeyAt(node_idx), buffer_pool_manager_);
      parent_node->SetKeyAt(node_idx, internal_sibling_node->KeyAt(0));
    }
    internal_sibling_node->IncreaseSize(-1);
    internal_node->IncreaseSize(1);
  }
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    throw std::runtime_error("error!root page id is NVALID_PAGE_ID!");
  }
  Page *root_page =
      buffer_pool_manager_->FetchPage(root_page_id_);  // 获取根结点page，此函数调用后会将获取到的page的pin_count++
  auto *page = reinterpret_cast<BPlusTreePage *>(root_page);
  while (!page->IsLeafPage()) {  // 只要不是叶子节点，则循环向下找
    auto *internal_page = reinterpret_cast<InternalPage *>(page);
    page_id_t next_id = internal_page->ValueAt(0);
    page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(next_id));
    // 这里记得将之前获取到的page进行unpin
    buffer_pool_manager_->UnpinPage(internal_page->GetPageId(), false);  // UnpinPgImp(page_id_t page_id, bool is_dirty)
  }

  return INDEXITERATOR_TYPE(reinterpret_cast<LeafPage *>(page), 0, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  auto *leaf_page = reinterpret_cast<LeafPage *>(FindLeafPage(key));
  return INDEXITERATOR_TYPE(leaf_page, leaf_page->KeyIndex(key, comparator_), buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    throw std::runtime_error("error!root page id is NVALID_PAGE_ID!");
  }
  Page *root_page =
      buffer_pool_manager_->FetchPage(root_page_id_);  // 获取根结点page，此函数调用后会将获取到的page的pin_count++
  auto *page = reinterpret_cast<BPlusTreePage *>(root_page);
  while (!page->IsLeafPage()) {  // 只要不是叶子节点，则循环向下找
    auto *internal_page = reinterpret_cast<InternalPage *>(page);
    page_id_t next_id = internal_page->ValueAt(internal_page->GetSize() - 1);
    page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(next_id));
    // 这里记得将之前获取到的page进行unpin
    buffer_pool_manager_->UnpinPage(internal_page->GetPageId(), false);  // UnpinPgImp(page_id_t page_id, bool is_dirty)
  }

  return INDEXITERATOR_TYPE(reinterpret_cast<LeafPage *>(page), page->GetSize(), buffer_pool_manager_);
}

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return root_page_id_; }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Draw an empty tree");
    return;
  }
  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  ToGraph(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm, out);
  out << "}" << std::endl;
  out.flush();
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  if (IsEmpty()) {
    LOG_WARN("Print an empty tree");
    return;
  }
  ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm);
}

/**
 * This method is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
