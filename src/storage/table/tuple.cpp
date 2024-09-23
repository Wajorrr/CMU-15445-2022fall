//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// tuple.cpp
//
// Identification: src/storage/table/tuple.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "storage/table/tuple.h"

namespace bustub {

// TODO(Amadou): It does not look like nulls are supported. Add a null bitmap?
// 根据输入的值和模式创建一个新的元组
Tuple::Tuple(std::vector<Value> values, const Schema *schema) : allocated_(true) {
  assert(values.size() == schema->GetColumnCount());

  // 1. Calculate the size of the tuple.
  // 获取元组的大小
  // 元组的大小包括模式的固定大小部分和所有非内联列的长度
  uint32_t tuple_size = schema->GetLength();
  // 遍历所有非内联列，计算元组的大小
  for (auto &i : schema->GetUnlinedColumns()) {
    auto len = values[i].GetLength();
    // 如果其长度为 BUSTUB_VALUE_NULL，则将长度设置为零
    if (len == BUSTUB_VALUE_NULL) {
      len = 0;
    }
    // 将列的长度加上一个 uint32_t 的大小（用于存储相对偏移量）累加到元组的总大小中
    tuple_size += (len + sizeof(uint32_t));
  }

  // 2. Allocate memory.
  size_ = tuple_size;
  data_ = new char[size_];
  std::memset(data_, 0, size_);

  // 3. Serialize each attribute based on the input value.
  // 序列化每个属性
  uint32_t column_count = schema->GetColumnCount();
  uint32_t offset = schema->GetLength();

  for (uint32_t i = 0; i < column_count; i++) {
    const auto &col = schema->GetColumn(i);

    if (!col.IsInlined()) {
      // Serialize relative offset, where the actual varchar data is stored.
      // 对于每个列，如果该列是非内联的，则首先序列化相对偏移量（即实际数据存储的位置）
      // 然后序列化实际的数据（包括大小和数据本身）
      // 将一个 uint32_t 类型的值写入到一个特定的内存位置
      // 将变量 offset 的值写入到 data_ 指针所指向的内存区域的某个偏移位置
      *reinterpret_cast<uint32_t *>(data_ + col.GetOffset()) = offset;
      // Serialize varchar value, in place (size+data).
      values[i].SerializeTo(data_ + offset);
      auto len = values[i].GetLength();
      // 如果列的长度为 BUSTUB_VALUE_NULL，则将长度设置为零
      if (len == BUSTUB_VALUE_NULL) {
        len = 0;
      }
      // 更新偏移量以指向下一个数据存储位置
      offset += (len + sizeof(uint32_t));
    } else {
      // 对于内联列，直接将数据序列化到固定位置
      values[i].SerializeTo(data_ + col.GetOffset());
    }
  }
}

Tuple::Tuple(const Tuple &other) : allocated_(other.allocated_), rid_(other.rid_), size_(other.size_) {
  if (allocated_) {
    delete[] data_;
  }
  if (allocated_) {
    // Deep copy.
    data_ = new char[size_];
    memcpy(data_, other.data_, size_);
  } else {
    // Shallow copy.
    data_ = other.data_;
  }
}

auto Tuple::operator=(const Tuple &other) -> Tuple & {
  if (allocated_) {
    delete[] data_;
  }
  allocated_ = other.allocated_;
  rid_ = other.rid_;
  size_ = other.size_;

  if (allocated_) {
    // Deep copy.
    data_ = new char[size_];
    memcpy(data_, other.data_, size_);
  } else {
    // Shallow copy.
    data_ = other.data_;
  }

  return *this;
}

auto Tuple::GetValue(const Schema *schema, const uint32_t column_idx) const -> Value {
  assert(schema);
  assert(data_);
  // 获取列的类型
  const TypeId column_type = schema->GetColumn(column_idx).GetType();
  // 获取列的数据指针
  const char *data_ptr = GetDataPtr(schema, column_idx);
  // the third parameter "is_inlined" is unused
  return Value::DeserializeFrom(data_ptr, column_type);
}

// 从现有元组中提取键元组
// 通过这种方式，KeyFromTuple 函数能够从现有元组中提取出一个包含特定键属性的子元组
// 这在数据库索引和键值对操作中非常有用
auto Tuple::KeyFromTuple(const Schema &schema, const Schema &key_schema,
                         const std::vector<uint32_t> &key_attrs) -> Tuple {
  // 首先创建一个 std::vector<Value> 对象 values，并预留空间以容纳所有键属性
  std::vector<Value> values;
  values.reserve(key_attrs.size());
  // 遍历 key_attrs 向量中的每个索引 idx
  // 并调用 this->GetValue(&schema, idx) 方法从当前元组中获取对应索引的值
  for (auto idx : key_attrs) {
    values.emplace_back(this->GetValue(&schema, idx));
  }
  // 返回一个新的 Tuple 对象，该对象由 values 向量和 key_schema 模式构造而成
  return {values, &key_schema};
}

auto Tuple::GetDataPtr(const Schema *schema, const uint32_t column_idx) const -> const char * {
  assert(schema);
  assert(data_);
  // 获取列
  const auto &col = schema->GetColumn(column_idx);
  // 判断列是否是内联的
  bool is_inlined = col.IsInlined();
  // For inline type, data is stored where it is.
  if (is_inlined) {
    // 如果列是内联的，则直接返回数据指针
    return (data_ + col.GetOffset());
  }
  // We read the relative offset from the tuple data.
  // 否则读取数据为相对偏移量
  int32_t offset = *reinterpret_cast<int32_t *>(data_ + col.GetOffset());
  // And return the beginning address of the real data for the VARCHAR type.
  // 返回实际数据的起始地址
  return (data_ + offset);
}

auto Tuple::ToString(const Schema *schema) const -> std::string {
  std::stringstream os;

  int column_count = schema->GetColumnCount();
  bool first = true;
  os << "(";
  for (int column_itr = 0; column_itr < column_count; column_itr++) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    if (IsNull(schema, column_itr)) {
      os << "<NULL>";
    } else {
      Value val = (GetValue(schema, column_itr));
      os << val.ToString();
    }
  }
  os << ")";
  os << " Tuple size is " << size_;

  return os.str();
}

void Tuple::SerializeTo(char *storage) const {
  memcpy(storage, &size_, sizeof(int32_t));
  memcpy(storage + sizeof(int32_t), data_, size_);
}

void Tuple::DeserializeFrom(const char *storage) {
  uint32_t size = *reinterpret_cast<const uint32_t *>(storage);
  // Construct a tuple.
  this->size_ = size;
  if (this->allocated_) {
    delete[] this->data_;
  }
  this->data_ = new char[this->size_];
  memcpy(this->data_, storage + sizeof(int32_t), this->size_);
  this->allocated_ = true;
}

}  // namespace bustub
