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
      internal_max_size_(internal_max_size) {
  std::cout << "leaf_max_size_: " << leaf_max_size_ << ", leaf_max_size_: " << leaf_max_size_ << "\n\n";
}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return root_page_id_ == INVALID_PAGE_ID; }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::MaxSize(BPlusTreePage *page) const -> int {
  return page->IsLeafPage() ? leaf_max_size_ - 1 : internal_max_size_;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsSafe(Page *page, Operation op) -> bool {  // 判断节点是否safe(在进行op操作后是否会分裂/合并)
  auto node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  if (op == INSERT) {  // 插入操作，判断节点插入一个值后是否会分裂(超过MaxSize)，分裂则可能会影响到祖先节点，not safe
    return node->GetSize() < MaxSize(node);
  }
  // 删除操作(读操作不需要判断节点是否safe)
  // if (node->GetPageId() == root_page_id_) {  // 当前节点为root节点
  if (node->GetParentPageId() == INVALID_PAGE_ID) {  // 当前节点为root节点
    if (node->IsLeafPage()) {
      return true;  // root节点且leaf节点，直接删除即可，safe
    }
    // root节点且internal节点，判断进行删除后索引个数是否为1(即当前size是否为2)
    // 为1则需要更新root节点，not safe，否则safe
    return node->GetSize() > 2;
  }
  // 非root节点，判断删除后是否达到minsize，达到则需要进行合并或调整，可能会影响到祖先节点，not safe
  return node->GetSize() > node->GetMinSize();
}
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::UnlockAndUnpin(Transaction *transaction, Operation op)
    -> void {  // 将当前pageset中所有已有的page解锁以及unpin
  if (transaction == nullptr) {
    return;
  }
  for (auto page : *transaction->GetPageSet()) {  // 遍历pageset，先解锁，然后unpin
    // 解锁和unpin的顺序不能变，先unpin再解锁的话可能unpin后page已经被驱逐出内存，page指针已经指向了未知对象
    if (op == READ) {
      // std::cout << "try RUnlatch:" << page->GetPageId() << "\n";
      page->RUnlatch();
      // std::cout << "RUnlatched:" << page->GetPageId() << "\n";
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    } else {
      // std::cout << "try WUnlatch:" << page->GetPageId() << "\n";
      page->WUnlatch();
      // std::cout << "WUnlatched:" << page->GetPageId() << "\n";
      buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    }
  }
  transaction->GetPageSet()->clear();
  for (auto page : *transaction->GetDeletedPageSet()) {  // 遍历delete pageset，统一对需要从内存中删除的page进行删除
    buffer_pool_manager_->DeletePage(page);
  }
  transaction->GetDeletedPageSet()->clear();
}
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
  // std::cout << "GetValue: key=" << key << "\n";
  if (IsEmpty()) {
    return false;
  }
  auto *page = FindLeafPageRW(key, 0, transaction, READ);
  if (page == nullptr) {
    return false;
  }
  auto leaf_page = reinterpret_cast<LeafPage *>(page->GetData());

  leaf_page->GetValue(key, comparator_, result);
  // 给定的key不存在，解锁，unpin，返回false
  if (transaction != nullptr) {
    UnlockAndUnpin(transaction, READ);
  } else {
    page->RUnlatch();
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  }
  // std::cout << "result:";
  //  for (auto i : *result) {
  //    std::cout << i << " ";
  //  }
  //  std::cout << "\n";
  return !result->empty();
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafPageRW(const KeyType &key, int left_right_most, Transaction *transaction, Operation op)
    -> Page * {
  // 给定key，返回key所在/要插入的leaf page
  if (IsEmpty()) {
    return nullptr;
  }
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  // 获取根结点page，此函数调用后会将获取到的page的pin_count++
  while (true) {
    // 由于涉及到并发，获取到root节点的锁后可能root节点已改变，所以需要循环获取root节点的锁
    if (page == nullptr) {  // 内存已满且page全pinned，fetch page失败
      throw std::runtime_error("buffer pool full!");
    }
    if (op == READ) {  // 读操作
      // std::cout << "try root Rlatch:" << page->GetPageId() << "\n";
      page->RLatch();
      // std::cout << "root Rlatched:" << page->GetPageId() << "\n";
    } else {  // 插入或者删除
      // std::cout << "try root Wlatch:" << page->GetPageId() << "\n";
      page->WLatch();
      // std::cout << "root Wlatched:" << page->GetPageId() << "\n";
    }
    if (transaction != nullptr) {
      transaction->AddIntoPageSet(page);
    }
    if (root_page_id_ == page->GetPageId()) {  // 如果是root节点，退出循环，继续后面的步骤
      break;
    }
    // 当前获取到锁的节点已经不是root节点了(之前是root节点)，则释放锁，重新获取root节点的锁
    if (transaction != nullptr) {
      UnlockAndUnpin(transaction, op);
    } else if (op == READ) {
      page->RUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    }
    page = buffer_pool_manager_->FetchPage(root_page_id_);
  }

  auto cur_page = reinterpret_cast<InternalPage *>(page->GetData());
  // 已获取到root节点的锁
  while (!cur_page->IsLeafPage()) {
    // 循环向下直到找到leaf node
    page_id_t next_id;
    if (left_right_most == 1) {
      next_id = cur_page->ValueAt(0);
    } else if (left_right_most == 2) {
      next_id = cur_page->ValueAt(cur_page->GetSize() - 1);
    } else {
      next_id = cur_page->ValueAt(cur_page->Search(key, comparator_));
    }
    Page *next_page = buffer_pool_manager_->FetchPage(next_id);
    if (op == READ) {  // 读操作
      // std::cout << "try read Rlatch:" << next_page->GetPageId() << "\n";
      next_page->RLatch();  // 给下一个节点加读锁，然后给上一个节点解锁
      // std::cout << "read Rlatched:" << next_page->GetPageId() << "\n";

      if (transaction != nullptr) {
        UnlockAndUnpin(transaction, op);
      } else {
        page->RUnlatch();
        buffer_pool_manager_->UnpinPage(cur_page->GetPageId(), false);
      }
    } else {  // 写操作，给下一个节点加写锁，判断下一个节点若安全，则给之前的所有节点解锁
      // std::cout << "try write Wlatch:" << next_page->GetPageId() << "\n";
      next_page->WLatch();
      // std::cout << "write Wlatched:" << next_page->GetPageId() << "\n";
      if (IsSafe(next_page, op)) {
        UnlockAndUnpin(transaction, op);
      }
    }
    if (transaction != nullptr) {
      transaction->AddIntoPageSet(next_page);  // 下一个节点加到pageset中
    }
    page = next_page;
    cur_page = reinterpret_cast<InternalPage *>(next_page->GetData());
  }
  return page;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafPage(const KeyType &key) -> Page * { return nullptr; }

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
  // std::cout << "insert: key=" << key << " , value=" << value << "\n";
  // std::cout << "IsEmpty:" << IsEmpty() << "\n";

  // 找到要insert的leaf page，此时已经获取了当前leaf node以及祖先的读锁
  auto *page = FindLeafPageRW(key, 0, transaction, INSERT);
  while (page == nullptr) {
    tree_latch_.lock();
    // 树为空，则需要给当前整个B+tree index上锁，然后新建根节点
    if (IsEmpty()) {
      page_id_t new_root_id;
      Page *new_page = buffer_pool_manager_->NewPage(&new_root_id);  // 从缓存申请一个新page，pin+1
      if (new_page == nullptr) {
        throw "no available frame";
      }

      auto *leaf_page = reinterpret_cast<LeafPage *>(new_page->GetData());
      leaf_page->Init(new_root_id, INVALID_PAGE_ID,
                      leaf_max_size_);  // 三个参数分别为当前page的page_id、父节点page的parent_id、叶子节点最大容量
      // leaf_page->Insert(key, value, comparator_);  // 插入key value对

      // std::cout << "inserted: key-{" << key << "} value-{" << value << "} getsize-" << leaf_page->GetSize() << "\n";
      root_page_id_ = new_root_id;  // 这里跟下面删除操作有个地方一样，都是不能过早更新root_id，否则会导致多线程下出错
      UpdateRootPageId(true);  // 更新root page id
      buffer_pool_manager_->UnpinPage(new_root_id,
                                      true);  // pin-1，由于向page中插入了kv对，需要设置page的dirty标记为true
      // return true;
    }
    tree_latch_.unlock();
    // 若树已经不为空，则可能已经被其他的插入操作先把树给建了，再次find leafpage
    page = FindLeafPageRW(key, 0, transaction, INSERT);
  }
  // 已找到leaf，且已给节点上锁
  auto *leaf_page = reinterpret_cast<LeafPage *>(page->GetData());

  if (leaf_page->GetValue(key, comparator_)) {  // 已存在key，插入失败
    UnlockAndUnpin(transaction, INSERT);
    return false;
  }
  // key不存在，插入
  leaf_page->Insert(key, value, comparator_);
  // std::cout << "inserted: key-{" << key << "} value-{" << value << "} getsize-" << leaf_page->GetSize()
  //           << "\n maxsize:" << leaf_page->GetMaxSize() << "\n";

  if (leaf_page->GetSize() >= leaf_max_size_) {  // 需要分裂
    auto new_leaf_page = reinterpret_cast<LeafPage *>(SplitNode(leaf_page, {}));
    InsertIntoParent(leaf_page, new_leaf_page->KeyAt(0), new_leaf_page);
    buffer_pool_manager_->UnpinPage(new_leaf_page->GetPageId(), true);
  }
  UnlockAndUnpin(transaction, INSERT);

  // std::cout << "current tree:\n";
  // ToString(reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)), buffer_pool_manager_);
  // buffer_pool_manager_->UnpinPage(root_page_id_, false);
  // std::cout << "\n\n";

  return true;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitNode(BPlusTreePage *node, std::pair<KeyType, page_id_t> child_item) -> BPlusTreePage * {
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);  // 申请new page，获取new_page_id
  if (new_page == nullptr) {
    throw "out of memory";
  }

  BPlusTreePage *new_node;

  if (node->IsLeafPage()) {                                                   // 若节点为leaf node
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);                     // 原node
    auto *new_leaf_node = reinterpret_cast<LeafPage *>(new_page->GetData());  // 新node
    new_leaf_node->Init(new_page_id, leaf_node->GetParentPageId(),
                        leaf_max_size_);  // Init(page_id_t page_id, page_id_t parent_id, int max_size)

    // 移动一半kv对到新的node中
    int move_num = (leaf_node->GetSize() + 1) / 2;  // 要分出去的kv对数量
    // 由于是flexible数组，因此应该从尾部分出一半kv对，即分出一个新的右边leaf node出来
    // 这样就可以直接通过减少原node的size来达到删除效果
    int start_pos = leaf_node->GetSize() - move_num;
    new_leaf_node->SplitCopy(leaf_node, start_pos, move_num);

    // 更新双向链表
    new_leaf_node->SetNextPageId(leaf_node->GetNextPageId());
    leaf_node->SetNextPageId(new_leaf_node->GetPageId());
    new_node = new_leaf_node;
  } else {  // 若节点为internalnode
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *new_internal_node = reinterpret_cast<InternalPage *>(new_page->GetData());
    new_internal_node->Init(new_page_id, internal_node->GetParentPageId(),
                            internal_max_size_);  // Init(page_id_t page_id, page_id_t parent_id, int max_size)

    new_internal_node->SplitCopy(internal_node, child_item, comparator_, buffer_pool_manager_);

    new_node = new_internal_node;
  }
  return new_node;  // 返回新建的节点指针
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *node, const KeyType &key, BPlusTreePage *new_node) {
  if (node->GetParentPageId() == INVALID_PAGE_ID) {  // 原节点已经是root节点，则需要新建root节点
    page_id_t new_root_id;
    Page *new_page = buffer_pool_manager_->NewPage(&new_root_id);  // 直接将root_page_id更新为新申请的page id
    auto *new_root_node = reinterpret_cast<InternalPage *>(new_page->GetData());

    new_root_node->Init(
        new_root_id, INVALID_PAGE_ID,
        internal_max_size_);  // 三个参数分别为当前page的page_id、父节点page的parent_id、internal节点最大容量
    // UpdateRootPageId(false);

    // 初始化新root节点的array_和size
    new_root_node->LinkToNewRoot(node->GetPageId(), key, new_node->GetPageId());

    // 将原节点和新节点均设为新root节点的子节点
    node->SetParentPageId(new_root_id);
    new_node->SetParentPageId(new_root_id);
    // 到这里才能更新root_page_id_，之前我是直接在new page的时候就更新了root_page_id
    // 导致新的root page还没初始化好，就被其他线程访问了，导致出错
    root_page_id_ = new_root_id;
    buffer_pool_manager_->UnpinPage(new_root_id, true);  // 对新建root节点进行了修改，dirty标记设置为true
  } else {
    page_id_t parent_page_id = node->GetParentPageId();
    auto *parent_node = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(parent_page_id)->GetData());

    if (parent_node->GetSize() < internal_max_size_) {
      // 将新节点插入到父节点的array中的相应位置 //这里不能直接插，会越界
      parent_node->NewNodeInsert(key, new_node->GetPageId(), comparator_);
      parent_node->IncreaseSize(1);
      new_node->SetParentPageId(parent_page_id);
    } else {  // 原来这里写的是 if (parent_node->GetSize() > internal_max_size_) {
      auto *parent_split_node = reinterpret_cast<InternalPage *>(SplitNode(parent_node, {key, new_node->GetPageId()}));
      // 注意，key取的是split后创建的新节点中idx为0的key，这个key在新节点中本身不会被使用到，但是是原节点和新节点的分界值
      InsertIntoParent(parent_node, parent_split_node->KeyAt(0), parent_split_node);
      buffer_pool_manager_->UnpinPage(parent_split_node->GetPageId(), true);
    }
    buffer_pool_manager_->UnpinPage(parent_page_id, true);
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
  // std::cout << "开始remove  " << key << std::endl;
  if (IsEmpty()) {
    return;
  }

  auto *leaf_page = FindLeafPageRW(key, 0, transaction, DELETE);
  if (leaf_page == nullptr) {  // 空树
    return;
  }

  auto leaf_node = reinterpret_cast<LeafPage *>(leaf_page);
  if (!leaf_node->GetValue(key, comparator_)) {  // 不存在key，删除失败
    UnlockAndUnpin(transaction, INSERT);
    return;
  }
  // std::cout << "size_pre:" << leaf_node->GetSize() << "\n";
  // std::cout << "root id:" << root_page_id_ << " remove at:" << leaf_node->GetPageId() << "\n";
  leaf_node->RemoveItem(key, comparator_);
  if (leaf_node->GetSize() < leaf_node->GetMinSize()) {
    CoalesceOrRedistribute(leaf_page, transaction);
    // DeleteEntryRW(leaf_page, key, transaction);
  }
  // std::cout << "size_after:" << leaf_node->GetSize() << ", page_id:" << leaf_node->GetPageId() << "\n";
  UnlockAndUnpin(transaction, DELETE);

  // std::cout << "current tree:\n";
  // ToString(reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)), buffer_pool_manager_);
  // buffer_pool_manager_->UnpinPage(root_page_id_, false);
  // std::cout << "\n\n";
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::CoalesceOrRedistribute(Page *page, Transaction *transaction) {
  auto node = reinterpret_cast<BPlusTreePage *>(page->GetData());

  if (node->GetParentPageId() == INVALID_PAGE_ID) {  // 根节点的情况单独考虑，这里也是递归更新的终点
    // std::cout << "AdjustRoot\n";

    AdjustRoot(page, transaction);
    return;
  }

  // 获取父节点以及兄弟节点的指针
  // auto *parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());  // parent_page pinned
  auto *parent_page = (*transaction->GetPageSet())[transaction->GetPageSet()->size() - 2];

  auto *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  int node_idx = parent_node->ValueIdx(node->GetPageId());
  // 默认取左边的兄弟节点来进行合并或调整，若当前节点为第一个节点，则取右边的兄弟节点
  int sibling_idx = node_idx - 1;  // 左边节点的idx
  if (node_idx == 0) {
    sibling_idx = node_idx + 1;  // 右边节点的idx
  }
  int sibling_page_id = parent_node->ValueAt(sibling_idx);

  // // std::cout << "sibling_page_id:" << sibling_page_id << "\n";
  // if (sibling_page_id == -1)  // 没有兄弟节点了
  // {
  //   std::cout << "no sibling page\n";
  //   parent_node->Remove(node_idx);
  //   transaction->GetPageSet()->pop_back();
  //   page->WUnlatch();
  //   buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
  //   buffer_pool_manager_->DeletePage(page->GetPageId());
  //   if (parent_node->GetSize() < parent_node->GetMinSize()) {
  //     CoalesceOrRedistribute(parent_page, transaction);  // 若父节点删除之后也小于minsize，则递归进行调整
  //   }
  //   return;
  // }

  // 获取兄弟节点，并将其加入page set
  Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_page_id);  // sibling_node pinned
  auto *sibling_node = reinterpret_cast<BPlusTreePage *>(sibling_page->GetData());
  // std::cout << "Lock sibling:" << sibling_page->GetPageId() << "\n";
  sibling_page->WLatch();
  // std::cout << "not dead\n";

  // transaction->AddIntoPageSet(sibling_page);

  // node和其左边节点的size之和不大于max size，则合并，注意internal page的maxsize和leaf page的maxsize是不同的
  if (node->GetSize() + sibling_node->GetSize() <= MaxSize(node)) {
    // std::cout << "Coalesce\n";
    if (node_idx == 0) {
      // std::cout << "swaped\n";
      // 若取的是node的右边节点进行合并，则将node和sibling互换，保证sibling在左node在右，将右边node节点合并到左边节点
      // std::swap(node, sibling_node);
      std::swap(node_idx, sibling_idx);
      Coalesce(page, sibling_page, parent_node, node_idx, transaction);
    } else {
      Coalesce(sibling_page, page, parent_node, node_idx, transaction);  // 将node合并到sibling_node
    }

    parent_node->Remove(node_idx);  // 删除父节点中node_idx对应的记录

    // sibling_page->WUnlatch();
    // std::cout << "unLock sibling:" << sibling_page->GetPageId() << "\n";
    // // buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);

    // if (swapped) {  // sibling节点被合并到当前节点了
    //   // std::cout << "delete:" << sibling_page->GetPageId() << "\n";
    //   // sibling_page->WUnlatch();
    //   buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);
    //   transaction->AddIntoDeletedPageSet(sibling_page->GetPageId());
    //   // buffer_pool_manager_->DeletePage(sibling_page->GetPageId());

    //   // transaction->GetDeletedPageSet()->erase(sibling_page->GetPageId());
    // } else {  // 当前节点被合并到sibling节点了
    //   // std::cout << "cur page id:" << page->GetPageId() << ",sibling page id:" << sibling_page->GetPageId();
    //   // std::cout << "\nPageSet:\n";
    //   // for (auto item : *transaction->GetPageSet()) {
    //   //   std::cout << "page:" << item->GetPageId() << ",";
    //   // }
    //   // std::cout << "\n";
    //   transaction->GetPageSet()->pop_back();
    //   // std::cout << "\nPageSet pop_back:\n";
    //   // for (auto item : *transaction->GetPageSet()) {
    //   //   std::cout << "page:" << item->GetPageId() << ",";
    //   // }
    //   // std::cout << "\n";
    //   page->WUnlatch();
    //   buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    //   // buffer_pool_manager_->DeletePage(page->GetPageId());

    //   transaction->AddIntoDeletedPageSet(page->GetPageId());
    // }

    if (parent_node->GetSize() < parent_node->GetMinSize()) {
      CoalesceOrRedistribute(parent_page, transaction);  // 若父节点删除之后也小于minsize，则递归进行调整
    }

    // buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
    return;
  }

  // std::cout << "Redistribute\n";
  // node和其兄弟节点的size之和超过max size，则从兄弟节点借kv对
  Redistribute(sibling_page, page, parent_node, node_idx, transaction);
  // sibling_page->WUnlatch();
  // std::cout << "unLock sibling:" << sibling_page->GetPageId() << "\n";
  // buffer_pool_manager_->UnpinPage(sibling_node->GetPageId(), true);

  // // 这里这样子写会死锁。。
  // // transaction->GetPageSet()->pop_back();

  // page->WUnlatch();
  // buffer_pool_manager_->UnpinPage(page->GetPageId(), true);

  // buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::AdjustRoot(Page *old_root_page, Transaction *transaction) {
  auto old_root_node = reinterpret_cast<BPlusTreePage *>(old_root_page->GetData());

  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    // 由于之后不会再用到这个page，直接将其从内存中删除，不用写回硬盘
    root_page_id_ = INVALID_PAGE_ID;
    transaction->GetPageSet()->pop_back();
    old_root_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(old_root_node->GetPageId(), true);
    buffer_pool_manager_->DeletePage(old_root_node->GetPageId());
    // transaction->AddIntoDeletedPageSet(old_root_node->GetPageId());
    UpdateRootPageId(false);
    return;
  }
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    // 删除根节点，把其唯一的子节点作为新的根节点
    auto internal_page = reinterpret_cast<InternalPage *>(old_root_node);
    page_id_t new_root_id = internal_page->ValueAt(0);
    internal_page->IncreaseSize(-1);

    auto *new_root_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(new_root_id)->GetData());
    new_root_page->SetParentPageId(INVALID_PAGE_ID);
    root_page_id_ = new_root_id;
    buffer_pool_manager_->UnpinPage(new_root_id, true);
    UpdateRootPageId(false);
    // std::cout << "root updated,size=" << new_root_page->GetSize() << "\n\n\n";

    // 从内存中删除原根节点对应的page

    transaction->GetPageSet()->pop_back();
    old_root_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(old_root_node->GetPageId(), true);
    buffer_pool_manager_->DeletePage(old_root_node->GetPageId());
    // transaction->AddIntoDeletedPageSet(internal_page->GetPageId());

    return;
  }

  // 正常从根节点删除key
  transaction->GetPageSet()->pop_back();
  old_root_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(old_root_page->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Coalesce(Page *sibling_page, Page *page, InternalPage *parent_node, int node_idx,
                              Transaction *transaction) {
  auto node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  auto sibling_node = reinterpret_cast<BPlusTreePage *>(sibling_page->GetData());
  if (node->IsLeafPage()) {  // leaf node合并
    // std::cout << "leaf Coalesce: " << node->GetPageId() << " to " << sibling->GetPageId() << "\n";
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *leaf_sibling_node = reinterpret_cast<LeafPage *>(sibling_node);
    leaf_sibling_node->SetNextPageId(leaf_node->GetNextPageId());  // 更新next指针
    leaf_node->MoveAll(leaf_sibling_node);
    leaf_sibling_node->IncreaseSize(leaf_node->GetSize());
    leaf_node->IncreaseSize(-1 * leaf_node->GetSize());
  } else {
    // std::cout << "internal Coalesce" << node->GetPageId() << " to " << sibling->GetPageId() << "\n";
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *internal_sibling_node = reinterpret_cast<InternalPage *>(sibling_node);
    internal_node->MoveAll(internal_sibling_node, parent_node->KeyAt(node_idx),
                           buffer_pool_manager_);  // 用parent node中指向当前node的key代替node中的第一个key,再进行合并
    internal_sibling_node->IncreaseSize(internal_node->GetSize());
    internal_node->IncreaseSize(-1 * internal_node->GetSize());
  }

  transaction->GetPageSet()->pop_back();

  sibling_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);

  // 原node已被合并到左边的节点，将原node对应的page删除
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
  buffer_pool_manager_->DeletePage(page->GetPageId());
  // transaction->AddIntoDeletedPageSet(page->GetPageId());
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Redistribute(Page *sibling_page, Page *page, InternalPage *parent_node, int node_idx,
                                  Transaction *transaction) {
  auto node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  auto sibling_node = reinterpret_cast<BPlusTreePage *>(sibling_page->GetData());
  if (sibling_node->IsLeafPage()) {  // leaf node
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *leaf_sibling_node = reinterpret_cast<LeafPage *>(sibling_node);
    if (node_idx == 0) {  // node左边没有节点了，即sibling node为右边的节点，将sibling node的第一个kv对移到node尾部
      // leaf_sibling_node->MoveFirst(leaf_node);
      // parent_node->SetKeyAt(node_idx + 1, leaf_sibling_node->KeyAt(0));

      ValueType first_value = leaf_sibling_node->ValueAt(0);
      KeyType first_key = leaf_sibling_node->KeyAt(0);
      leaf_sibling_node->Delete(first_key, comparator_);

      // std::cout << "redistribute from right page:" << leaf_sibling_node->GetPageId()
      //           << " to left page:" << leaf_node->GetPageId() << ", key=" << first_key << "\n\n";

      leaf_node->InsertLast(first_key, first_value);
      parent_node->SetKeyAt(node_idx + 1, leaf_sibling_node->KeyAt(0));
    } else {  // sibling node在node节点的左边，将sibling node的最后一个kv对移到node头部
      // leaf_sibling_node->MoveLast(leaf_node);
      // parent_node->SetKeyAt(node_idx, leaf_node->KeyAt(0));  // 更新父节点的索引

      ValueType last_value = leaf_sibling_node->ValueAt(leaf_sibling_node->GetSize() - 1);
      KeyType last_key = leaf_sibling_node->KeyAt(leaf_sibling_node->GetSize() - 1);
      leaf_sibling_node->IncreaseSize(-1);
      leaf_node->InsertFirst(last_key, last_value);
      parent_node->SetKeyAt(node_idx, last_key);
    }
    // leaf_sibling_node->IncreaseSize(-1);
    // leaf_node->IncreaseSize(1);
  } else {  // internal node
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *internal_sibling_node = reinterpret_cast<InternalPage *>(sibling_node);
    if (node_idx == 0) {  // sibling node在右
                          // 注意需要将sibling_node将被移动的第一个kv对的key值更新，并将value值对应的child的parent更新
      // internal_sibling_node->MoveFirst(internal_node, parent_node->KeyAt(node_idx + 1), buffer_pool_manager_);
      // parent_node->SetKeyAt(node_idx + 1, internal_sibling_node->KeyAt(0));

      page_id_t first_value = internal_sibling_node->ValueAt(0);
      KeyType first_key = internal_sibling_node->KeyAt(1);
      internal_sibling_node->DeleteFirst();

      internal_node->Insert(std::make_pair(parent_node->KeyAt(node_idx + 1), first_value), comparator_);
      auto child_page = buffer_pool_manager_->FetchPage(first_value);
      auto child_node = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child_node->SetParentPageId(internal_node->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
      parent_node->SetKeyAt(node_idx + 1, first_key);

    } else {  // sibling node在左
      // 注意需要将node的第一个kv对的key值更新，并将sibling_node的最后一个value值对应的child的parent更新
      // internal_sibling_node->MoveLast(internal_node, parent_node->KeyAt(node_idx), buffer_pool_manager_);
      // parent_node->SetKeyAt(node_idx, internal_node->KeyAt(0));

      page_id_t last_value = internal_sibling_node->ValueAt(internal_sibling_node->GetSize() - 1);
      KeyType last_key = internal_sibling_node->KeyAt(internal_sibling_node->GetSize() - 1);
      internal_sibling_node->IncreaseSize(-1);
      internal_node->InsertFirst(parent_node->KeyAt(node_idx), last_value);

      auto child_page = buffer_pool_manager_->FetchPage(last_value);
      auto child_node = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child_node->SetParentPageId(internal_node->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);

      parent_node->SetKeyAt(node_idx, last_key);
    }
    // internal_sibling_node->IncreaseSize(-1);
    // internal_node->IncreaseSize(1);
  }

  transaction->GetPageSet()->pop_back();

  sibling_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);

  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DeleteEntry(Page *&page, const KeyType &key) -> void {}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DeleteEntryRW(Page *&page, const KeyType &key, Transaction *transaction) -> void {}
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
    return INDEXITERATOR_TYPE();
  }
  auto first_page = FindLeafPageRW(KeyType{}, 1, nullptr, READ);
  // std::cout << "Begin()，first_page_id:" << first_page->GetPageId() << "\n";
  return INDEXITERATOR_TYPE(first_page, 0, first_page->GetPageId(), buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return INDEXITERATOR_TYPE();
  }
  auto leaf_page = FindLeafPageRW(key, 0, nullptr, READ);

  auto leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  int index;
  for (index = 0; index < leaf_node->GetSize(); index++) {
    if (comparator_(leaf_node->KeyAt(index), key) == 0) {
      break;
    }
  }
  if (index == leaf_node->GetSize()) {
    leaf_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return End();
  }
  return INDEXITERATOR_TYPE(leaf_page, index, leaf_page->GetPageId(), buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return INDEXITERATOR_TYPE();
  }
  auto last_page = FindLeafPageRW(KeyType{}, 2, nullptr, READ);
  auto last_node = reinterpret_cast<LeafPage *>(last_page->GetData());
  last_page->RUnlatch();
  buffer_pool_manager_->UnpinPage(last_page->GetPageId(), false);
  // std::cout << "End()，root_page_id:" << root_page_id_ << ", last_page_id:" << last_page->GetPageId()
  //           << " page size:" << last_node->GetSize() << "\n";
  return INDEXITERATOR_TYPE(last_page, last_node->GetSize(), last_page->GetPageId(), buffer_pool_manager_);
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
