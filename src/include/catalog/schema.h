//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// schema.h
//
// Identification: src/include/catalog/schema.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "catalog/column.h"
#include "common/exception.h"
#include "type/type.h"

namespace bustub {

class Schema;
using SchemaRef = std::shared_ptr<const Schema>;

// Schema 类用于描述表的结构，即表中的列及其属性
// 它包含了表中所有列的信息，并提供了一些方法来查询和操作这些列
class Schema {
 public:
  /**
   * Constructs the schema corresponding to the vector of columns, read left-to-right.
   * @param columns columns that describe the schema's individual columns
   */
  // 根据传入的列向量构造一个 Schema 对象
  explicit Schema(const std::vector<Column> &columns);

  // 从另一个 Schema 对象中复制指定的列，生成一个新的 Schema 对象
  static auto CopySchema(const Schema *from, const std::vector<uint32_t> &attrs) -> Schema {
    std::vector<Column> cols;
    cols.reserve(attrs.size());
    for (const auto i : attrs) {
      cols.emplace_back(from->columns_[i]);
    }
    return Schema{cols};
  }

  /** @return all the columns in the schema */
  // 返回 Schema 中所有列的向量
  auto GetColumns() const -> const std::vector<Column> & { return columns_; }

  /**
   * Returns a specific column from the schema.
   * @param col_idx index of requested column
   * @return requested column
   */
  // 返回指定索引的列
  auto GetColumn(const uint32_t col_idx) const -> const Column & { return columns_[col_idx]; }

  /**
   * Looks up and returns the index of the first column in the schema with the specified name.
   * If multiple columns have the same name, the first such index is returned.
   * @param col_name name of column to look for
   * @return the index of a column with the given name, throws an exception if it does not exist
   */
  // 返回指定列名的索引，如果不存在则抛出异常
  auto GetColIdx(const std::string &col_name) const -> uint32_t {
    if (auto col_idx = TryGetColIdx(col_name)) {
      return *col_idx;
    }
    UNREACHABLE("Column does not exist");
  }

  /**
   * Looks up and returns the index of the first column in the schema with the specified name.
   * If multiple columns have the same name, the first such index is returned.
   * @param col_name name of column to look for
   * @return the index of a column with the given name, `std::nullopt` if it does not exist
   */
  // 返回指定列名的索引，如果不存在则返回 std::nullopt
  auto TryGetColIdx(const std::string &col_name) const -> std::optional<uint32_t> {
    for (uint32_t i = 0; i < columns_.size(); ++i) {
      if (columns_[i].GetName() == col_name) {
        return std::optional{i};
      }
    }
    return std::nullopt;
  }

  /** @return the indices of non-inlined columns */
  // 返回所有非内联列的索引
  auto GetUnlinedColumns() const -> const std::vector<uint32_t> & { return uninlined_columns_; }

  /** @return the number of columns in the schema for the tuple */
  // 返回 Schema 中的列总数
  auto GetColumnCount() const -> uint32_t { return static_cast<uint32_t>(columns_.size()); }

  /** @return the number of non-inlined columns */
  // 返回非内联列的总数
  auto GetUnlinedColumnCount() const -> uint32_t { return static_cast<uint32_t>(uninlined_columns_.size()); }

  /** @return the number of bytes used by one tuple */
  // 返回一个元组的总长度（字节数）
  inline auto GetLength() const -> uint32_t { return length_; }

  /** @return true if all columns are inlined, false otherwise */
  // 返回是否所有列都是内联的
  inline auto IsInlined() const -> bool { return tuple_is_inlined_; }

  /** @return string representation of this schema */
  auto ToString(bool simplified = true) const -> std::string;

 private:
  /** Fixed-length column size, i.e. the number of bytes used by one tuple. */
  uint32_t length_;  // 元组的固定长度

  /** All the columns in the schema, inlined and uninlined. */
  std::vector<Column> columns_;  // 所有列的向量

  /** True if all the columns are inlined, false otherwise. */
  bool tuple_is_inlined_{true};  // 是否所有列都是内联的

  /** Indices of all uninlined columns. */
  std::vector<uint32_t> uninlined_columns_;  // 非内联列的索引
};

}  // namespace bustub

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<bustub::Schema, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const bustub::Schema &x, FormatCtx &ctx) const {
    return fmt::formatter<std::string>::format(x.ToString(), ctx);
  }
};

template <typename T>
struct fmt::formatter<std::shared_ptr<T>, std::enable_if_t<std::is_base_of<bustub::Schema, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const std::shared_ptr<T> &x, FormatCtx &ctx) const {
    if (x != nullptr) {
      return fmt::formatter<std::string>::format(x->ToString(), ctx);
    }
    return fmt::formatter<std::string>::format("", ctx);
  }
};

template <typename T>
struct fmt::formatter<std::unique_ptr<T>, std::enable_if_t<std::is_base_of<bustub::Schema, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const std::unique_ptr<T> &x, FormatCtx &ctx) const {
    if (x != nullptr) {
      return fmt::formatter<std::string>::format(x->ToString(), ctx);
    }
    return fmt::formatter<std::string>::format("", ctx);
  }
};
