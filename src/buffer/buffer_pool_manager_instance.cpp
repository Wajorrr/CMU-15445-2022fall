//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager_instance.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager_instance.h"

#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  page_table_ = new ExtendibleHashTable<page_id_t, frame_id_t>(bucket_size_);
  replacer_ = new LRUKReplacer(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
  // LOG_DEBUG("init_replacer_size:%ld", replacer_->Size());
  // TODO(students): remove this line after you have implemented the buffer pool manager
  // throw NotImplementedException(
  //     "BufferPoolManager is not implemented yet. If you have finished implementing BPM, please remove the throw "
  //     "exception line in `buffer_pool_manager_instance.cpp`.");
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
  delete page_table_;
  delete replacer_;
}

auto BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  // LOG_DEBUG("111");

  frame_id_t frame_id;

  if (!free_list_.empty()) {  // 有空闲的frame，获取一个空闲的frame_id即可
    frame_id = free_list_.front();
    free_list_.pop_front();
    // LOG_DEBUG("free_list_size:%ld", free_list_.size());

  } else {  // 没有空闲的frame了
    // LOG_DEBUG("replacer_size:%ld", replacer_->Size());
    if (!replacer_->Evict(&frame_id)) {
      // LOG_DEBUG("333");
      return nullptr;
    }
    // 这里不仅要获取驱逐出数据的frame的id，还得判断旧page是否被修改过，修改过则需要先写出到disk
    // 然后还得把旧page的data清除，把hash表中原来的<k,v>删除

    // 找到旧的page，若旧的page已经被修改的话，就得先将其写入到文件中
    // 这里我的理解出了问题，本以为pages_是通过page_id来索引page，而extendible hashtable又是page_id到frame_id的映射
    // 这样的话就没办法通过frame_id来找到这个frame对应的旧page的id了
    // 导致我还考虑了再添加一个extendible hashtable(这样就得修改这个类中已经实现好的构造方法了)，
    // 或者为extendible hashtable添加通过value查找key的方法(这样又得考虑key和value是不是一定是一对一映射)
    // 这样以至于我一度陷入焦灼状态。。然后去偷偷看了下GitHub上别人的实现，原来pages_是通过frame_id来索引page的
    // 这样的话pages_和bufferpool就是一个一一对应的关系了
    // 主要原因可能还是自己对frame和page这两个东西的关系不太熟悉吧，不过也只看了下别人代码中的这一小部分0.0
    Page &old_page = pages_[frame_id];
    if (old_page.IsDirty()) {
      disk_manager_->WritePage(old_page.page_id_, old_page.GetData());
    }
    old_page.ResetMemory();                          // 将page中原来的数据清除
    page_table_->Remove(pages_[frame_id].page_id_);  // 将hash表中的对应<k,v>删除
  }

  // LOG_DEBUG("frame_id:%d", frame_id);

  // 到这里就都一样了，初始化page和frame，并将它们的映射插入hash表
  *page_id = AllocatePage();
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);  // 标记当前frame正在被使用

  // 将frame对应的page的page_id更新为当前的page_id(BufferPoolManagerInstance类是page类的友元类，因此可以直接访问其私有成员)
  // 除此之外还得标记当前有进程正在使用这个page，以及初始化page的dirty标志为false
  pages_[frame_id].page_id_ = *page_id;
  pages_[frame_id].pin_count_ = 1;
  pages_[frame_id].is_dirty_ = false;

  // LOG_DEBUG("page_id:%d,frame_id:%d", *page_id, frame_id);
  page_table_->Insert(*page_id, frame_id);

  return &pages_[frame_id];
}

auto BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (page_table_->Find(page_id, frame_id)) {  // page在buffer pool中已存在
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    pages_[frame_id].pin_count_++;
    return &pages_[frame_id];
  }

  // Page不在buffer pool中，需要把当前page绑定一个空闲的frame或者驱逐page之后的frame
  if (!free_list_.empty()) {  // 有空闲的frame，获取一个空闲的frame_id即可
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {                               // 没有空闲的frame了
    if (!replacer_->Evict(&frame_id)) {  // page不在buffer pool中，且buffer pool已满，其中的frame全部不可驱逐
      return nullptr;
    }

    Page &old_page = pages_[frame_id];
    if (old_page.IsDirty()) {
      disk_manager_->WritePage(old_page.page_id_, old_page.GetData());
    }
    old_page.ResetMemory();                          // 将page中原来的数据清除
    page_table_->Remove(pages_[frame_id].page_id_);  // 将hash表中的对应<k,v>删除
  }

  // 到这里就都一样了，初始化page和frame，并将它们的映射插入hash表
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);  // 标记当前frame正在被使用

  // 将frame对应的page的page_id更新为当前的page_id(BufferPoolManagerInstance类是page类的友元类，因此可以直接访问其私有成员)
  // 除此之外还得标记当前有进程正在使用这个page，以及初始化page的dirty标志为false
  pages_[frame_id].page_id_ = page_id;
  pages_[frame_id].pin_count_ = 1;
  pages_[frame_id].is_dirty_ = false;

  page_table_->Insert(page_id, frame_id);

  disk_manager_->ReadPage(page_id, pages_[frame_id].GetData());  // 从文件中读取data
  return &pages_[frame_id];
}

auto BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  // LOG_DEBUG("222");
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return false;
  }
  if (pages_[frame_id].GetPinCount() == 0) {
    return false;
  }

  if (is_dirty) {
    pages_[frame_id].is_dirty_ = is_dirty;
  }

  pages_[frame_id].pin_count_--;
  if (pages_[frame_id].pin_count_ == 0) {
    replacer_->SetEvictable(frame_id, true);
  }
  return true;
}

auto BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);

  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return false;
  }

  disk_manager_->WritePage(page_id, pages_[frame_id].GetData());
  pages_[frame_id].is_dirty_ = false;
  return true;
}

void BufferPoolManagerInstance::FlushAllPgsImp() {
  for (size_t i = 0; i < pool_size_; i++) {
    FlushPage(pages_[i].GetPageId());
  }
}

auto BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return true;
  }
  if (pages_[frame_id].GetPinCount() > 0) {
    return false;
  }

  // pages_[frame_id].page_id_ = INVALID_PAGE_ID;
  // pages_[frame_id].pin_count_ = 0;
  // pages_[frame_id].is_dirty_ = false;
  pages_[frame_id].ResetMemory();

  DeallocatePage(page_id);
  page_table_->Remove(page_id);
  replacer_->Remove(frame_id);
  free_list_.emplace_back(frame_id);
  return true;
}

auto BufferPoolManagerInstance::AllocatePage() -> page_id_t { return next_page_id_++; }

}  // namespace bustub
