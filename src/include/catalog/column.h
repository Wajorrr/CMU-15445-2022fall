//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// column.h
//
// Identification: src/include/catalog/column.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "fmt/format.h"

#include "common/exception.h"
#include "common/macros.h"
#include "type/type.h"

namespace bustub {
class AbstractExpression;

// Column 类在 BusTub 数据库系统中用于描述表中的单个列
// 它包含了列的名称、类型、长度、偏移量等信息，并提供了一些方法来查询这些属性
class Column {
  friend class Schema;

 public:
  /**
   * Non-variable-length constructor for creating a Column.
   * @param column_name name of the column
   * @param type type of the column
   */
  // 用于创建非变长列（如整数、布尔值等）
  // 该构造函数要求列的类型不能是 VARCHAR，因为 VARCHAR 是变长类型
  Column(std::string column_name, TypeId type)
      : column_name_(std::move(column_name)), column_type_(type), fixed_length_(TypeSize(type)) {
    BUSTUB_ASSERT(type != TypeId::VARCHAR, "Wrong constructor for VARCHAR type.");
  }

  /**
   * Variable-length constructor for creating a Column.
   * @param column_name name of the column
   * @param type type of column
   * @param length length of the varlen
   * @param expr expression used to create this column
   */
  // 用于创建变长列（如 VARCHAR）
  // 该构造函数要求列的类型必须是 VARCHAR，并且需要指定列的最大长度
  Column(std::string column_name, TypeId type, uint32_t length)
      : column_name_(std::move(column_name)),
        column_type_(type),
        fixed_length_(TypeSize(type)),
        variable_length_(length) {
    BUSTUB_ASSERT(type == TypeId::VARCHAR, "Wrong constructor for non-VARCHAR type.");
  }

  /**
   * Replicate a Column with a different name.
   * @param column_name name of the column
   * @param column the original column
   */
  // 用于复制一个已有的列，但可以指定新的列名
  Column(std::string column_name, const Column &column)
      : column_name_(std::move(column_name)),
        column_type_(column.column_type_),
        fixed_length_(column.fixed_length_),
        variable_length_(column.variable_length_),
        column_offset_(column.column_offset_) {}

  /** @return column name */
  // 返回列的名称
  auto GetName() const -> std::string { return column_name_; }

  /** @return column length */
  // 返回列的长度。如果列是内联的（非变长），则返回固定长度；否则返回变长列的长度
  auto GetLength() const -> uint32_t {
    if (IsInlined()) {
      return fixed_length_;
    }
    return variable_length_;
  }

  /** @return column fixed length */
  // 返回列的固定长度。对于非变长列，这是列的实际长度；对于变长列，这是指针的大小
  auto GetFixedLength() const -> uint32_t { return fixed_length_; }

  /** @return column variable length */
  // 返回变长列的长度。对于非变长列，该值为 0
  auto GetVariableLength() const -> uint32_t { return variable_length_; }

  /** @return column's offset in the tuple */
  // 返回列在元组中的偏移量
  auto GetOffset() const -> uint32_t { return column_offset_; }

  /** @return column type */
  // 返回列的数据类型
  auto GetType() const -> TypeId { return column_type_; }

  /** @return true if column is inlined, false otherwise */
  // 判断列是否为内联列。如果列的类型不是 VARCHAR，则为内联列
  auto IsInlined() const -> bool { return column_type_ != TypeId::VARCHAR; }

  /** @return a string representation of this column */
  // 返回列的字符串表示。simplified 参数用于控制是否简化输出
  auto ToString(bool simplified = true) const -> std::string;

 private:
  /**
   * Return the size in bytes of the type.
   * @param type type whose size is to be determined
   * @return size in bytes
   */
  // 获取数据类型的大小
  static auto TypeSize(TypeId type) -> uint8_t {
    switch (type) {
      case TypeId::BOOLEAN:
      case TypeId::TINYINT:
        return 1;
      case TypeId::SMALLINT:
        return 2;
      case TypeId::INTEGER:
        return 4;
      case TypeId::BIGINT:
      case TypeId::DECIMAL:
      case TypeId::TIMESTAMP:
        return 8;
      case TypeId::VARCHAR:
        // TODO(Amadou): Confirm this.
        return 12;
      default: {
        UNREACHABLE("Cannot get size of invalid type");
      }
    }
  }

  /** Column name. */
  // 标识列的名称
  std::string column_name_;

  /** Column value's type. */
  // 列的数据类型。TypeId 是一个枚举类型，用于表示列的数据类型，例如整数、浮点数、字符串等
  TypeId column_type_;

  /** For a non-inlined column, this is the size of a pointer. Otherwise, the size of the fixed length column. */
  // 对于非内联列，这是指针的大小；对于内联列，这是固定长度列的大小
  // 内联列是指那些数据直接存储在元组中的列，而非内联列的数据存储在元组之外，通过指针引用
  uint32_t fixed_length_;

  /** For an inlined column, 0. Otherwise, the length of the variable length column. */
  // 对于内联列，该值为 0；对于非内联列，该值表示可变长度列的长度
  // 可变长度列是指那些长度不固定的列，例如字符串或二进制数据
  uint32_t variable_length_{0};

  /** Column offset in the tuple. */
  // 列在元组中的偏移量
  uint32_t column_offset_{0};
};

}  // namespace bustub

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<bustub::Column, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const bustub::Column &x, FormatCtx &ctx) const {
    return fmt::formatter<std::string>::format(x.ToString(), ctx);
  }
};

template <typename T>
struct fmt::formatter<std::unique_ptr<T>, std::enable_if_t<std::is_base_of<bustub::Column, T>::value, char>>
    : fmt::formatter<std::string> {
  template <typename FormatCtx>
  auto format(const std::unique_ptr<bustub::Column> &x, FormatCtx &ctx) const {
    return fmt::formatter<std::string>::format(x->ToString(), ctx);
  }
};
