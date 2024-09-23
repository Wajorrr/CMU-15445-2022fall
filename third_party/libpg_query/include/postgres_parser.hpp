//===----------------------------------------------------------------------===//
//                         DuckDB
//
// postgres_parser.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <vector>
#include "nodes/pg_list.hpp"
#include "pg_simplified_token.hpp"

namespace duckdb {
// PostgresParser 类主要负责解析 SQL 查询，并提供了一些辅助功能，如标记化和关键字检查
class PostgresParser {
 public:
  PostgresParser();
  ~PostgresParser();

  bool success;
  duckdb_libpgquery::PGList *parse_tree;
  std::string error_message;
  int error_location;

 public:
  // Parse 方法接受一个 SQL 查询字符串，并使用 duckdb_libpgquery 库进行解析。解析结果存储在 res 变量中
  void Parse(const std::string &query);
  // Tokenize 方法用于将 SQL 查询字符串标记化，同样使用 duckdb_libpgquery 库，并返回一个包含标记的向量
  static std::vector<duckdb_libpgquery::PGSimplifiedToken> Tokenize(const std::string &query);

  // 检查给定的字符串是否是 SQL 关键字
  static bool IsKeyword(const std::string &text);
  // 返回一个包含所有 SQL 关键字的向量
  static std::vector<duckdb_libpgquery::PGKeyword> KeywordList();
  // 设置是否保留标识符的大小写
  static void SetPreserveIdentifierCase(bool downcase);
};

}  // namespace duckdb
