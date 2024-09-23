//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// bustub_instance.h
//
// Identification: src/include/common/bustub_instance.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "catalog/catalog.h"
#include "common/config.h"
#include "common/util/string_util.h"
#include "libfort/lib/fort.hpp"
#include "type/value.h"

namespace bustub {

class Transaction;
class ExecutorContext;
class DiskManager;
class BufferPoolManager;
class LockManager;
class TransactionManager;
class LogManager;
class CheckpointManager;
class Catalog;
class ExecutionEngine;

class ResultWriter {
 public:
  ResultWriter() = default;
  virtual ~ResultWriter() = default;

  virtual void WriteCell(const std::string &cell) = 0;
  virtual void WriteHeaderCell(const std::string &cell) = 0;
  virtual void BeginHeader() = 0;
  virtual void EndHeader() = 0;
  virtual void BeginRow() = 0;
  virtual void EndRow() = 0;
  virtual void BeginTable(bool simplified_output) = 0;
  virtual void EndTable() = 0;

  bool simplified_output_{false};
};

class NoopWriter : public ResultWriter {
 public:
  NoopWriter() = default;
  void WriteCell(const std::string &cell) override {}
  void WriteHeaderCell(const std::string &cell) override {}
  void BeginHeader() override {}
  void EndHeader() override {}
  void BeginRow() override {}
  void EndRow() override {}
  void BeginTable(bool simplified_output) override {}
  void EndTable() override {}
};

class SimpleStreamWriter : public ResultWriter {
 public:
  explicit SimpleStreamWriter(std::ostream &stream, bool disable_header = false, const char *separator = "\t")
      : disable_header_(disable_header), stream_(stream), separator_(separator) {}
  static auto BoldOn(std::ostream &os) -> std::ostream & { return os << "\e[1m"; }
  static auto BoldOff(std::ostream &os) -> std::ostream & { return os << "\e[0m"; }
  void WriteCell(const std::string &cell) override { stream_ << cell << separator_; }
  void WriteHeaderCell(const std::string &cell) override {
    if (!disable_header_) {
      stream_ << BoldOn << cell << BoldOff << separator_;
    }
  }
  void BeginHeader() override {}
  void EndHeader() override {
    if (!disable_header_) {
      stream_ << std::endl;
    }
  }
  void BeginRow() override {}
  void EndRow() override { stream_ << std::endl; }
  void BeginTable(bool simplified_output) override {}
  void EndTable() override {}

  bool disable_header_;
  std::ostream &stream_;
  std::string separator_;
};

class HtmlWriter : public ResultWriter {
  auto Escape(const std::string &data) -> std::string {
    std::string buffer;
    buffer.reserve(data.size());
    for (const char &ch : data) {
      switch (ch) {
        case '&':
          buffer.append("&amp;");
          break;
        case '\"':
          buffer.append("&quot;");
          break;
        case '\'':
          buffer.append("&apos;");
          break;
        case '<':
          buffer.append("&lt;");
          break;
        case '>':
          buffer.append("&gt;");
          break;
        default:
          buffer.push_back(ch);
          break;
      }
    }
    return buffer;
  }

 public:
  void WriteCell(const std::string &cell) override {
    std::cout << cell;
    if (!simplified_output_) {
      ss_ << "<td>" << Escape(cell) << "</td>";
    } else {
      ss_ << Escape(cell);
    }
  }
  void WriteHeaderCell(const std::string &cell) override {
    if (!simplified_output_) {
      ss_ << "<td>" << Escape(cell) << "</td>";
    } else {
      ss_ << Escape(cell);
    }
  }
  void BeginHeader() override {
    if (!simplified_output_) {
      ss_ << "<thead><tr>";
    }
  }
  void EndHeader() override {
    if (!simplified_output_) {
      ss_ << "</tr></thead>";
    }
  }
  void BeginRow() override {
    if (!simplified_output_) {
      ss_ << "<tr>";
    }
  }
  void EndRow() override {
    if (!simplified_output_) {
      ss_ << "</tr>";
    }
  }
  void BeginTable(bool simplified_output) override {
    simplified_output_ = simplified_output;
    if (!simplified_output_) {
      ss_ << "<table>";
    } else {
      ss_ << "<div>";
    }
  }
  void EndTable() override {
    if (!simplified_output_) {
      ss_ << "</table>";
    } else {
      ss_ << "</div>";
    }
  }
  std::stringstream ss_;
};

// 将查询结果格式化为表格形式，并使用 fort::utf8_table 库来生成表格
class FortTableWriter : public ResultWriter {
 public:
  // 用于向当前单元格写入数据
  void WriteCell(const std::string &cell) override { table_ << cell; }
  // 用于向表头单元格写入数据
  void WriteHeaderCell(const std::string &cell) override { table_ << cell; }
  // 用于开始表头的写入过程
  void BeginHeader() override { table_ << fort::header; }
  // 用于结束表头的写入过程
  void EndHeader() override { table_ << fort::endr; }
  // 用于开始一行的写入过程
  void BeginRow() override {}
  // 用于结束一行的写入过程
  void EndRow() override { table_ << fort::endr; }
  // 用于开始表格的写入过程
  // 接收一个布尔参数 simplified_output，如果该参数为真
  // 则设置表格的边框样式为 FT_EMPTY_STYLE，表示简化输出
  void BeginTable(bool simplified_output) override {
    if (simplified_output) {
      table_.set_border_style(FT_EMPTY_STYLE);
    }
  }
  // 用于结束表格的写入过程
  // 将当前表格转换为字符串并存储在 tables_ 向量中，然后重置 table_ 对象以便开始新的表格
  void EndTable() override {
    tables_.emplace_back(table_.to_string());
    table_ = fort::utf8_table{};
  }
  fort::utf8_table table_;
  std::vector<std::string> tables_;
};

// 用于管理和操作数据库实例的核心类
class BustubInstance {
 private:
  /**
   * Get the executor context from the BusTub instance.
   */
  // 用于从 BustubInstance 实例中获取执行上下文
  // 接收一个事务对象 txn 作为参数，并返回一个 ExecutorContext 的唯一指针
  auto MakeExecutorContext(Transaction *txn) -> std::unique_ptr<ExecutorContext>;

 public:
  // 根据提供的数据库文件创建 BustubInstance 实例
  explicit BustubInstance(const std::string &db_file_name);

  BustubInstance();

  ~BustubInstance();

  /**
   * Execute a SQL query in the BusTub instance.
   */
  // 用于在 BustubInstance 实例中执行 SQL 查询
  auto ExecuteSql(const std::string &sql, ResultWriter &writer) -> bool;

  /**
   * Execute a SQL query in the BusTub instance with provided txn.
   */
  // 用于在 BustubInstance 实例中执行带有事务的 SQL 查询
  auto ExecuteSqlTxn(const std::string &sql, ResultWriter &writer, Transaction *txn) -> bool;

  /**
   * FOR TEST ONLY. Generate test tables in this BusTub instance.
   * It's used in the shell to predefine some tables, as we don't support
   * create / drop table and insert for now. Should remove it in the future.
   */
  // 仅用于测试的函数，用于在 BustubInstance 实例中生成测试表
  void GenerateTestTable();

  /**
   * FOR TEST ONLY. Generate mock tables in this BusTub instance.
   * It's used in the shell to predefine some tables, as we don't support
   * create / drop table and insert for now. Should remove it in the future.
   */
  // 仅用于测试的函数，用于在 BustubInstance 实例中生成模拟表
  void GenerateMockTable();

  // TODO(chi): change to unique_ptr. Currently they're directly referenced by recovery test, so
  // we cannot do anything on them until someone decides to refactor the recovery test.

  DiskManager *disk_manager_;               // 磁盘管理器
  BufferPoolManager *buffer_pool_manager_;  // 缓冲池管理器
  LockManager *lock_manager_;               // 锁管理器
  TransactionManager *txn_manager_;         // 事务管理器
  LogManager *log_manager_;                 // 日志管理器
  CheckpointManager *checkpoint_manager_;   // 检查点管理器
  Catalog *catalog_;                        // 目录
  ExecutionEngine *execution_engine_;       // 执行引擎
  std::shared_mutex catalog_lock_;

  // 用于获取会话变量的值。如果会话变量存在，则返回其值；否则返回空字符串
  auto GetSessionVariable(const std::string &key) -> std::string {
    if (session_variables_.find(key) != session_variables_.end()) {
      return session_variables_[key];
    }
    return "";
  }

  // 用于判断是否强制使用初始优化规则
  auto IsForceStarterRule() -> bool {
    auto variable = StringUtil::Lower(GetSessionVariable("force_optimizer_starter_rule"));
    return variable == "1" || variable == "true" || variable == "yes";
  }

 private:
  // 用于显示数据库中的表
  void CmdDisplayTables(ResultWriter &writer);
  // 用于显示数据库中的索引
  void CmdDisplayIndices(ResultWriter &writer);
  // 用于显示帮助信息
  void CmdDisplayHelp(ResultWriter &writer);
  // 用于向结果写入一个单元格
  void WriteOneCell(const std::string &cell, ResultWriter &writer);
  std::unordered_map<std::string, std::string> session_variables_;
};

}  // namespace bustub
