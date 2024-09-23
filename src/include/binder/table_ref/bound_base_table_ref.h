#pragma once

#include <optional>
#include <string>
#include <utility>
#include "binder/bound_table_ref.h"
#include "catalog/schema.h"
#include "concurrency/transaction.h"
#include "fmt/core.h"

namespace bustub {

/**
 * A bound table ref type for single table. e.g., `SELECT x FROM y`, where `y` is `BoundBaseTableRef`.
 */
class BoundBaseTableRef : public BoundTableRef {
 public:
  explicit BoundBaseTableRef(std::string table, table_oid_t oid, std::optional<std::string> alias, Schema schema)
      : BoundTableRef(TableReferenceType::BASE_TABLE),
        table_(std::move(table)),
        oid_(oid),
        alias_(std::move(alias)),
        schema_(std::move(schema)) {}

  auto ToString() const -> std::string override {
    // 首先检查 alias_ 是否为空（std::nullopt），如果为空，则返回不包含别名的字符串表示形式；
    // 否则，返回包含别名的字符串表示形式
    if (alias_ == std::nullopt) {
      return fmt::format("BoundBaseTableRef {{ table={}, oid={} }}", table_, oid_);
    }
    return fmt::format("BoundBaseTableRef {{ table={}, oid={}, alias={} }}", table_, oid_, *alias_);
  }

  // 返回一个 std::string 类型的值，用于获取绑定表的名称
  auto GetBoundTableName() const -> std::string {
    if (alias_ != std::nullopt) {
      return *alias_;
    }
    return table_;
  }

  /** The name of the table. */
  // 表的名称
  std::string table_;

  /** The oid of the table. */
  // 表的 OID
  table_oid_t oid_;

  /** The alias of the table */
  // 表的别名
  std::optional<std::string> alias_;

  /** The schema of the table. */
  // 表的模式
  Schema schema_;
};
}  // namespace bustub
