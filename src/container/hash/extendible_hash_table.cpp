//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_hash_table.cpp
//
// Identification: src/container/hash/extendible_hash_table.cpp
//
// Copyright (c) 2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdlib>
#include <functional>
#include <list>
#include <string>
#include <utility>

#include "common/logger.h"
#include "container/hash/extendible_hash_table.h"
#include "storage/page/page.h"

namespace bustub {

template <typename K, typename V>
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size) : bucket_size_(bucket_size) {
  // global_depth_ = 0;
  // num_buckets_ = 1;
  dir_.push_back(std::make_shared<Bucket>(bucket_size, 0));
  dir_[0]->SetIdx(0);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) -> size_t {
  int mask = (1 << global_depth_) - 1;

  // std::cout << "key:" << key << " hash:" << std::hash<K>()(key) << "," << (std::hash<K>()(key) & mask) << "\n";

  return std::hash<K>()(key) & mask;  // 获取hash值末尾对应global_depth位数的数，作为要放入的bucket的序号
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetGlobalDepthInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepthInternal() const -> int {
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int dir_index) const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetLocalDepthInternal(dir_index);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepthInternal(int dir_index) const -> int {
  return dir_[dir_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetNumBucketsInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBucketsInternal() const -> int {
  return num_buckets_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value)
    -> bool {  // 根据hash function得到key对应的bucket index，然后在相应的bucket中进行Find
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Find(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key)
    -> bool {  // 根据hash function得到key对应的bucket index，然后在相应的bucket中进行Remove
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Remove(key);
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::InsertInternal(const K &key, const V &value) {
  int idx = IndexOf(key);
  // std::cout << "InsertInternal key:" << key << " idx:" << idx << "\n";
  if (dir_[idx]->Insert(key, value)) {  // 更新或者插入成功
    return;
  }
  // LOG_DEBUG("LocalDepth(%d):%d,GlobalDepth:%d", idx, GetLocalDepthInternal(idx), GetGlobalDepthInternal());
  // 插入失败，bucket已满，需要扩容

  if (GetLocalDepthInternal(idx) ==
      GetGlobalDepthInternal()) {  // LocalDepth=GlobalDepth，global和local均++，然后新建bucket重分配
    int dir_len = 1 << global_depth_;
    // LOG_DEBUG("dir_len:%d", dir_len);
    for (int i = dir_len; i < dir_len * 2; i++) {
      // 将global目录扩大一倍，扩大的一倍仍然映射到已有的bucket上，只有溢出的bucket需要增大local_depth并新建bucket
      dir_.push_back(std::shared_ptr<Bucket>(dir_[i - dir_len]));
    }
    dir_[idx]->IncrementDepth();
    global_depth_++;

    // LOG_DEBUG("idx+dir_len:%d,localDepth:%d", idx + dir_len, GetLocalDepthInternal(idx));
    // 新建
    dir_[idx + dir_len] = std::make_shared<Bucket>(bucket_size_, GetLocalDepthInternal(idx));
    dir_[idx + dir_len]->SetIdx(idx + dir_len);
    num_buckets_++;
    // 重分配
    RedistributeBucket(dir_[idx]);

    // retry
    InsertInternal(key, value);

  } else {  // local_depth<global_depth，增大LocalDepth，直接新建bucket重分配，不用增大GlobalDepth
    int old_bucket_idx = dir_[idx]->GetIdx();

    // 此时可能有多个idx映射到同一个桶，但是不知道idx是不是对应自己的桶
    // 根据当前桶的idx以及LocalDepth来确定要在哪个idx来新建桶
    int new_bucket_idx = old_bucket_idx + (1 << GetLocalDepthInternal(idx));

    dir_[idx]->IncrementDepth();  // 增大原桶的LocalDepth

    // 新建桶
    dir_[new_bucket_idx] = std::make_shared<Bucket>(bucket_size_, GetLocalDepthInternal(idx));
    dir_[new_bucket_idx]->SetIdx(new_bucket_idx);
    num_buckets_++;

    if (GetLocalDepthInternal(idx) !=
        GetGlobalDepthInternal()) {  // 如果增大LocalDepth之后仍小于GlobalDepth，则需要重建与这两个桶相关的索引
      int global_len = 1 << GetGlobalDepthInternal();
      int local_len = 1 << GetLocalDepthInternal(idx);
      for (int i = new_bucket_idx + 1; i < global_len; i++) {
        if ((i & (local_len - 1)) == old_bucket_idx) {
          dir_[i] = std::shared_ptr<Bucket>(dir_[old_bucket_idx]);
        } else if ((i & (local_len - 1)) == new_bucket_idx) {
          dir_[i] = std::shared_ptr<Bucket>(dir_[new_bucket_idx]);
        }
      }
    }  // 如果增大LocalDepth之后等于GlobalDepth，则不需要重建索引

    // 重分配原桶
    RedistributeBucket(dir_[old_bucket_idx]);

    // retry
    InsertInternal(key, value);
  }
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) {
  std::scoped_lock<std::mutex> lock(latch_);
  InsertInternal(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::RedistributeBucket(std::shared_ptr<Bucket> bucket) -> void {
  auto &items = bucket->GetItems();
  auto iter = items.begin();
  while (iter != items.end()) {
    int idx = IndexOf(iter->first);
    // LOG_DEBUG("distribute idx:%d", idx);
    if (dir_[idx] != bucket) {
      auto temp = iter;
      iter++;
      InsertInternal(temp->first, temp->second);
      bucket->Remove(temp->first);
    } else {
      iter++;
    }
  }
}

//===--------------------------------------------------------------------===//
// Bucket
//===--------------------------------------------------------------------===//
template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t array_size, int depth) : size_(array_size), depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value)
    -> bool {  // 遍历bucket的list，找相应的<K,V> pair，找不到则返回false
  for (auto &item : list_) {
    if (item.first == key) {
      value = item.second;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key)
    -> bool {  // 遍历bucket的list，找相应的<K,V> pair并删除，找不到则返回false
  V value;
  if (Find(key, value)) {
    list_.remove({key, value});
    return true;
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value)
    -> bool {  // 找到相应的<K,V> pair则更新value并返回true，否则判断bucket是否满，满则返回false，否则插入并返回true
  // LOG_DEBUG("bucket current size:%ld", list_.size());
  V temp;
  if (Find(key, temp)) {
    list_.remove({key, temp});
    list_.push_back({key, value});
    return true;
  }

  if (IsFull()) {
    return false;
  }

  list_.push_back({key, value});
  return true;
}

template class ExtendibleHashTable<page_id_t, Page *>;
template class ExtendibleHashTable<Page *, std::list<Page *>::iterator>;
template class ExtendibleHashTable<int, int>;
// test purpose
template class ExtendibleHashTable<int, std::string>;
template class ExtendibleHashTable<int, std::list<int>::iterator>;

}  // namespace bustub
