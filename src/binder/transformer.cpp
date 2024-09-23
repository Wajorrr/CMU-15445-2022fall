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

#include <memory>
#include "binder/binder.h"
#include "binder/bound_expression.h"
#include "binder/bound_order_by.h"
#include "binder/bound_statement.h"
#include "binder/statement/create_statement.h"
#include "binder/statement/delete_statement.h"
#include "binder/statement/explain_statement.h"
#include "binder/statement/index_statement.h"
#include "binder/statement/insert_statement.h"
#include "binder/statement/select_statement.h"
#include "binder/statement/update_statement.h"
#include "binder/table_ref/bound_base_table_ref.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/util/string_util.h"
#include "nodes/nodes.hpp"
#include "nodes/parsenodes.hpp"
#include "type/decimal_type.h"

namespace bustub {

// 保存解析树中的节点，PGList 是一个链表结构，用于存储解析树中的节点
void Binder::SaveParseTree(duckdb_libpgquery::PGList *tree) {
  std::vector<std::unique_ptr<BoundStatement>> statements;
  // 遍历 tree 链表中的所有节点
  for (auto entry = tree->head; entry != nullptr; entry = entry->next) {
    // 将当前节点的数据指针 entry->data.ptr_value 解释为 duckdb_libpgquery::PGNode 类型的指针
    // 并将其添加到 statement_nodes_ 向量中
    statement_nodes_.push_back(reinterpret_cast<duckdb_libpgquery::PGNode *>(entry->data.ptr_value));
  }
}

// 根据传入的 SQL 语句节点类型，将其绑定为相应的 BoundStatement 对象
auto Binder::BindStatement(duckdb_libpgquery::PGNode *stmt) -> std::unique_ptr<BoundStatement> {
  switch (stmt->type) {
    case duckdb_libpgquery::T_PGRawStmt:  // 存储单个 SQL 语句原始解析树
      return BindStatement(reinterpret_cast<duckdb_libpgquery::PGRawStmt *>(stmt)->stmt);
    case duckdb_libpgquery::T_PGCreateStmt:  // 创建表
      return BindCreate(reinterpret_cast<duckdb_libpgquery::PGCreateStmt *>(stmt));
    case duckdb_libpgquery::T_PGInsertStmt:  // 插入数据
      return BindInsert(reinterpret_cast<duckdb_libpgquery::PGInsertStmt *>(stmt));
    case duckdb_libpgquery::T_PGSelectStmt:  // 查询数据
      return BindSelect(reinterpret_cast<duckdb_libpgquery::PGSelectStmt *>(stmt));
    case duckdb_libpgquery::T_PGExplainStmt:  // 解释执行计划
      return BindExplain(reinterpret_cast<duckdb_libpgquery::PGExplainStmt *>(stmt));
    case duckdb_libpgquery::T_PGDeleteStmt:  // 删除数据
      return BindDelete(reinterpret_cast<duckdb_libpgquery::PGDeleteStmt *>(stmt));
    case duckdb_libpgquery::T_PGUpdateStmt:  // 更新数据
      return BindUpdate(reinterpret_cast<duckdb_libpgquery::PGUpdateStmt *>(stmt));
    case duckdb_libpgquery::T_PGIndexStmt:  // 创建索引
      return BindIndex(reinterpret_cast<duckdb_libpgquery::PGIndexStmt *>(stmt));
    case duckdb_libpgquery::T_PGVariableSetStmt:  // 设置变量
      return BindVariableSet(reinterpret_cast<duckdb_libpgquery::PGVariableSetStmt *>(stmt));
    case duckdb_libpgquery::T_PGVariableShowStmt:  // 显示变量
      return BindVariableShow(reinterpret_cast<duckdb_libpgquery::PGVariableShowStmt *>(stmt));
    default:
      throw NotImplementedException(NodeTagToString(stmt->type));
  }
}

}  // namespace bustub
