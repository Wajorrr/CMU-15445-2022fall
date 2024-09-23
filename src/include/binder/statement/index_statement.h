//===----------------------------------------------------------------------===//
//                         BusTub
//
// binder/index_statement.h
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "binder/bound_statement.h"
#include "binder/expressions/bound_column_ref.h"
#include "binder/table_ref/bound_base_table_ref.h"
#include "catalog/column.h"

namespace bustub {

//// 用于表示 SQL 中的创建索引语句
class IndexStatement : public BoundStatement {
 public:
  // 构造函数是显式的（explicit），这意味着它不能用于隐式转换
  explicit IndexStatement(std::string index_name, std::unique_ptr<BoundBaseTableRef> table,
                          std::vector<std::unique_ptr<BoundColumnRef>> cols);

  /** Name of the index */
  // 一个表示索引名称的 std::string 类型参数 index_name
  std::string index_name_;

  /** Create on which table */
  // 一个表示目标表的 std::unique_ptr<BoundBaseTableRef> 类型参数 table
  std::unique_ptr<BoundBaseTableRef> table_;

  /** Name of the columns */
  // 表示列集合的 std::vector<std::unique_ptr<BoundColumnRef>> 类型参数 cols
  std::vector<std::unique_ptr<BoundColumnRef>> cols_;

  auto ToString() const -> std::string override;
};

}  // namespace bustub
