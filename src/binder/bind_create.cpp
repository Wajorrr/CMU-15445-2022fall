//===----------------------------------------------------------------------===//
// Copyright 2018-2022 Stichting DuckDB Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice (including the next paragraph)
// shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//===----------------------------------------------------------------------===//

#include <iterator>
#include <memory>
#include <string>
#include "binder/binder.h"
#include "binder/bound_expression.h"
#include "binder/bound_statement.h"
#include "binder/expressions/bound_agg_call.h"
#include "binder/expressions/bound_binary_op.h"
#include "binder/expressions/bound_column_ref.h"
#include "binder/expressions/bound_constant.h"
#include "binder/expressions/bound_star.h"
#include "binder/expressions/bound_unary_op.h"
#include "binder/statement/create_statement.h"
#include "binder/statement/index_statement.h"
#include "binder/statement/select_statement.h"
#include "binder/table_ref/bound_base_table_ref.h"
#include "binder/table_ref/bound_cross_product_ref.h"
#include "binder/table_ref/bound_join_ref.h"
#include "binder/tokens.h"
#include "catalog/catalog.h"
#include "common/exception.h"
#include "common/util/string_util.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "nodes/nodes.hpp"
#include "nodes/primnodes.hpp"
#include "pg_definitions.hpp"
#include "postgres_parser.hpp"
#include "type/type_id.h"

namespace bustub {

// 用于处理 SQL 中的列定义，并将其转换为内部的 Column 对象
auto Binder::BindColumnDefinition(duckdb_libpgquery::PGColumnDef *cdef) -> Column {
  std::string colname;
  // 从 cdef 参数中提取列名，并将其存储在 colname 变量中
  if (cdef->colname != nullptr) {
    colname = cdef->colname;
  }
  // 检查 cdef->collClause 是否为空。如果不为空，
  // 则抛出 NotImplementedException 异常，因为当前不支持列上的 collClause
  if (cdef->collClause != nullptr) {
    throw NotImplementedException("coll clause on column is not supported");
  }
  // 从 cdef->typeName 中提取列的数据类型名称，并将其存储在 name 变量中
  // 通过类型转换将 cdef->typeName->names->tail->data.ptr_value
  // 转换为 duckdb_libpgquery::PGValue 类型，并获取其字符串值
  auto name = std::string(
      (reinterpret_cast<duckdb_libpgquery::PGValue *>(cdef->typeName->names->tail->data.ptr_value)->val.str));

  // 如果名称为 "int4"，则返回一个 Column 对象，列名为 colname，类型为 TypeId::INTEGER
  if (name == "int4") {
    return {colname, TypeId::INTEGER};
  }

  // 如果名称为 "varchar"，则函数调用 BindExpressionList 方法绑定 cdef->typeName->typmods 表达式列表
  if (name == "varchar") {
    auto exprs = BindExpressionList(cdef->typeName->typmods);
    // 如果表达式列表的大小不为 1，则抛出 bustub::Exception 异常，表明应为 varchar 字段指定最大长度
    if (exprs.size() != 1) {
      throw bustub::Exception("should specify max length for varchar field");
    }
    // 将表达式列表中的第一个表达式转换为 BoundConstant 类型
    const auto &varchar_max_length_val = dynamic_cast<const BoundConstant &>(*exprs[0]);
    // 并获取其字符串值，转换为 uint32_t 类型的 varchar_max_length
    uint32_t varchar_max_length = std::stoi(varchar_max_length_val.ToString());
    // 返回一个 Column 对象，列名为 colname，类型为 TypeId::VARCHAR，最大长度为 varchar_max_length
    return {colname, TypeId::VARCHAR, varchar_max_length};
  }

  // 如果数据类型名称不为 "int4" 或 "varchar"，则抛出 NotImplementedException 异常，表明不支持该类型
  throw NotImplementedException(fmt::format("unsupported type: {}", name));
}

// 用于处理 SQL 中的 CREATE TABLE 语句，并将其转换为内部的 CreateStatement 对象
auto Binder::BindCreate(duckdb_libpgquery::PGCreateStmt *pg_stmt) -> std::unique_ptr<CreateStatement> {
  // 从 pg_stmt 参数中提取表名，并将其存储在 table 变量中
  auto table = std::string(pg_stmt->relation->relname);
  // 初始化一个空的 columns 向量，用于存储表的列定义，并初始化 column_count 变量为 0，用于记录列的数量
  auto columns = std::vector<Column>{};
  size_t column_count = 0;

  // 遍历 pg_stmt->tableElts 链表中的每个节点
  for (auto c = pg_stmt->tableElts->head; c != nullptr; c = lnext(c)) {
    // 每个节点都包含一个指向 duckdb_libpgquery::PGNode 的指针，根据节点的类型进行处理
    auto node = reinterpret_cast<duckdb_libpgquery::PGNode *>(c->data.ptr_value);
    switch (node->type) {
      case duckdb_libpgquery::T_PGColumnDef: {
        // 对于 duckdb_libpgquery::T_PGColumnDef 类型的节点
        // 函数将其转换为 duckdb_libpgquery::PGColumnDef 类型
        // 并调用 BindColumnDefinition 方法将其绑定为列定义
        auto cdef = reinterpret_cast<duckdb_libpgquery::PGColumnDef *>(c->data.ptr_value);
        auto centry = BindColumnDefinition(cdef);
        // 当前不支持约束
        if (cdef->constraints != nullptr) {
          throw NotImplementedException("constraints not supported");
        }
        // 成功绑定列定义后，将其添加到 columns 向量中，并增加 column_count 计数
        columns.push_back(std::move(centry));
        column_count++;
        break;
      }
      case duckdb_libpgquery::T_PGConstraint: {  // 当前不支持约束
        throw NotImplementedException("constraints not supported");
        break;
      }
      default:  // 其他类型的节点，抛出 NotImplementedException 异常，尚未处理这些类型的节点
        throw NotImplementedException("ColumnDef type not handled yet");
    }
  }
  // 表中至少应有一列
  if (column_count == 0) {
    throw bustub::Exception("should have at least 1 column");
  }
  // 返回一个包含表名和列定义的 CreateStatement 对象
  return std::make_unique<CreateStatement>(std::move(table), std::move(columns));
}

// 将一个 PGIndexStmt 类型的指针 stmt 绑定到一个 IndexStatement 对象
// 并返回一个指向该对象的唯一指针（std::unique_ptr<IndexStatement>）
auto Binder::BindIndex(duckdb_libpgquery::PGIndexStmt *stmt) -> std::unique_ptr<IndexStatement> {
  // 存储索引列的引用
  std::vector<std::unique_ptr<BoundColumnRef>> cols;
  // table 变量表示要创建索引的基础表引用
  auto table = BindBaseTableRef(stmt->relation->relname, std::nullopt);

  // 遍历 stmt->indexParams 链表中的每一个节点
  for (auto cell = stmt->indexParams->head; cell != nullptr; cell = cell->next) {
    // 对于每个节点，函数将其数据部分转换为 PGIndexElem 类型的指针 index_element
    auto index_element = reinterpret_cast<duckdb_libpgquery::PGIndexElem *>(cell->data.ptr_value);
    if (index_element->name != nullptr) {
      // 调用 ResolveColumn 函数，传入 table 和包含列名的向量，来解析列引用
      auto column_ref = ResolveColumn(*table, std::vector{std::string(index_element->name)});
      // 转换为 BoundColumnRef 类型，并通过 std::make_unique 创建一个唯一指针，添加到 cols 向量中
      cols.emplace_back(std::make_unique<BoundColumnRef>(dynamic_cast<const BoundColumnRef &>(*column_ref)));
    } else {
      throw NotImplementedException("create index by expr is not supported yet");
    }
  }
  // 使用 std::make_unique 创建一个 IndexStatement 对象
  return std::make_unique<IndexStatement>(stmt->idxname, std::move(table), std::move(cols));
}

}  // namespace bustub
