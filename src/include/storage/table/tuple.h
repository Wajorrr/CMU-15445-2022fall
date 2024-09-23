//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// tuple.h
//
// Identification: src/include/storage/table/tuple.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <vector>

#include "catalog/schema.h"
#include "common/rid.h"
#include "type/value.h"

namespace bustub {

/**
 * Tuple format:
 * ---------------------------------------------------------------------
 * | FIXED-SIZE or VARIED-SIZED OFFSET | PAYLOAD OF VARIED-SIZED FIELD |
 * ---------------------------------------------------------------------
 */
// 表示数据库表中的一行数据
// 格式包括固定大小或可变大小的偏移量，以及可变大小字段的有效载荷
class Tuple {
  friend class TablePage;
  friend class TableHeap;
  friend class TableIterator;

 public:
  // Default constructor (to create a dummy tuple)
  Tuple() = default;

  // constructor for table heap tuple
  explicit Tuple(RID rid) : rid_(rid) {}

  // constructor for creating a new tuple based on input value
  // 根据输入的值和模式创建一个新的元组
  Tuple(std::vector<Value> values, const Schema *schema);

  // copy constructor, deep copy
  Tuple(const Tuple &other);

  // assign operator, deep copy
  auto operator=(const Tuple &other) -> Tuple &;

  ~Tuple() {
    if (allocated_) {
      delete[] data_;
    }
    allocated_ = false;
    data_ = nullptr;
  }
  // 序列化和反序列化元组数据
  //  serialize tuple data
  void SerializeTo(char *storage) const;

  // deserialize tuple data(deep copy)
  void DeserializeFrom(const char *storage);

  // 获取元组的 RID、数据地址和长度
  //  return RID of current tuple
  inline auto GetRid() const -> RID { return rid_; }

  // Get the address of this tuple in the table's backing store
  inline auto GetData() const -> char * { return data_; }

  // Get length of the tuple, including varchar legth
  inline auto GetLength() const -> uint32_t { return size_; }

  // Get the value of a specified column (const)
  // checks the schema to see how to return the Value.
  // 获取指定列的值
  auto GetValue(const Schema *schema, uint32_t column_idx) const -> Value;

  // Generates a key tuple given schemas and attributes
  // 生成键元组
  auto KeyFromTuple(const Schema &schema, const Schema &key_schema, const std::vector<uint32_t> &key_attrs) -> Tuple;

  // Is the column value null ?
  // 检查列值是否为空
  inline auto IsNull(const Schema *schema, uint32_t column_idx) const -> bool {
    Value value = GetValue(schema, column_idx);
    return value.IsNull();
  }
  inline auto IsAllocated() -> bool { return allocated_; }

  // 将元组转换为字符串表示
  auto ToString(const Schema *schema) const -> std::string;

 private:
  // Get the starting storage address of specific column
  auto GetDataPtr(const Schema *schema, uint32_t column_idx) const -> const char *;

  // 指示元组是否已分配内存
  bool allocated_{false};  // is allocated?
  // 行标识符，如果元组指向表堆，则该值有效
  RID rid_{};  // if pointing to the table heap, the rid is valid
  // 元组的大小
  uint32_t size_{0};
  // 指向元组数据的指针
  char *data_{nullptr};
};

}  // namespace bustub
