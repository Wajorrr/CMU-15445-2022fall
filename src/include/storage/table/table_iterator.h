//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// table_iterator.h
//
// Identification: src/include/storage/table/table_iterator.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>

#include "common/rid.h"
#include "concurrency/transaction.h"
#include "storage/table/tuple.h"

namespace bustub {

class TableHeap;

/**
 * TableIterator enables the sequential scan of a TableHeap.
 */
// 用于顺序扫描 TableHeap
class TableIterator {
  // 友元类 Cursor
  friend class Cursor;

 public:
  TableIterator(TableHeap *table_heap, RID rid, Transaction *txn);

  TableIterator(const TableIterator &other)
      : table_heap_(other.table_heap_), tuple_(new Tuple(*other.tuple_)), txn_(other.txn_) {}

  ~TableIterator() { delete tuple_; }

  // 比较两个迭代器是否相等或不等，基于元组的资源 ID
  inline auto operator==(const TableIterator &itr) const -> bool {
    return tuple_->rid_.Get() == itr.tuple_->rid_.Get();
  }

  inline auto operator!=(const TableIterator &itr) const -> bool { return !(*this == itr); }

  // operator* 和 operator-> 提供了对元组的解引用和成员访问
  auto operator*() -> const Tuple &;

  auto operator->() -> Tuple *;

  // 前置和后置递增运算符 (operator++ 和 operator++(int)) 用于移动迭代器到下一个元组
  auto operator++() -> TableIterator &;

  auto operator++(int) -> TableIterator;

  // 赋值运算符 (operator=) 通过深拷贝元组和更新其他成员变量来实现迭代器的赋值操作
  auto operator=(const TableIterator &other) -> TableIterator & {
    table_heap_ = other.table_heap_;
    *tuple_ = *other.tuple_;
    txn_ = other.txn_;
    return *this;
  }

 private:
  TableHeap *table_heap_;
  Tuple *tuple_;
  Transaction *txn_;
};

}  // namespace bustub
