#include <optional>
#include <shared_mutex>
#include <string>
#include <tuple>

#include "binder/binder.h"
#include "binder/bound_expression.h"
#include "binder/bound_statement.h"
#include "binder/statement/create_statement.h"
#include "binder/statement/explain_statement.h"
#include "binder/statement/index_statement.h"
#include "binder/statement/select_statement.h"
#include "binder/statement/set_show_statement.h"
#include "buffer/buffer_pool_manager_instance.h"
#include "catalog/schema.h"
#include "catalog/table_generator.h"
#include "common/bustub_instance.h"
#include "common/enums/statement_type.h"
#include "common/exception.h"
#include "common/util/string_util.h"
#include "concurrency/lock_manager.h"
#include "concurrency/transaction.h"
#include "execution/execution_engine.h"
#include "execution/executor_context.h"
#include "execution/executors/mock_scan_executor.h"
#include "execution/expressions/abstract_expression.h"
#include "execution/plans/abstract_plan.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "optimizer/optimizer.h"
#include "planner/planner.h"
#include "recovery/checkpoint_manager.h"
#include "recovery/log_manager.h"
#include "storage/disk/disk_manager.h"
#include "storage/disk/disk_manager_memory.h"
#include "type/value_factory.h"

namespace bustub {

// 用于创建一个执行上下文（ExecutorContext），它包含了事务、目录、缓冲池管理器、事务管理器和锁管理器等信息
auto BustubInstance::MakeExecutorContext(Transaction *txn) -> std::unique_ptr<ExecutorContext> {
  return std::make_unique<ExecutorContext>(txn, catalog_, buffer_pool_manager_, txn_manager_, lock_manager_);
}

// 接收一个数据库文件名 db_file_name 作为参数，并根据该文件名初始化数据库实例
BustubInstance::BustubInstance(const std::string &db_file_name) {
  enable_logging = false;

  // Storage related.
  // 创建一个 DiskManager 对象，用于管理磁盘上的数据存储
  disk_manager_ = new DiskManager(db_file_name);

  // Log related.
  // 创建一个 LogManager 对象，用于管理数据库的日志记录
  log_manager_ = new LogManager(disk_manager_);

  // We need more frames for GenerateTestTable to work. Therefore, we use 128 instead of the default
  // buffer pool size specified in `config.h`.
  try {
    // 创建一个 BufferPoolManagerInstance 对象，用于管理内存中的缓冲池
    // 128 个缓冲页，使用 LRU-K 替换策略，日志管理器 log_manager_ 用于日志记录
    buffer_pool_manager_ = new BufferPoolManagerInstance(128, disk_manager_, LRUK_REPLACER_K, log_manager_);
  } catch (NotImplementedException &e) {
    std::cerr << "BufferPoolManager is not implemented, only mock tables are supported." << std::endl;
    buffer_pool_manager_ = nullptr;
  }

  // Transaction (txn) related.
  // 创建一个 LockManager 对象和一个 TransactionManager 对象，用于管理数据库事务
  lock_manager_ = new LockManager();
  txn_manager_ = new TransactionManager(lock_manager_, log_manager_);

  // Checkpoint related.
  // 创建一个 CheckpointManager 对象，用于管理数据库的检查点
  checkpoint_manager_ = new CheckpointManager(txn_manager_, log_manager_, buffer_pool_manager_);

  // Catalog.
  // 创建了一个 Catalog 对象，用于管理数据库的元数据
  catalog_ = new Catalog(buffer_pool_manager_, lock_manager_, log_manager_);

  // Execution engine.
  // 创建了一个 ExecutionEngine 对象，用于执行查询计划
  execution_engine_ = new ExecutionEngine(buffer_pool_manager_, txn_manager_, catalog_);
}

BustubInstance::BustubInstance() {
  // 禁用日志记录功能
  enable_logging = false;

  // Storage related.
  disk_manager_ = new DiskManagerUnlimitedMemory();

  // Log related.
  log_manager_ = new LogManager(disk_manager_);

  // We need more frames for GenerateTestTable to work. Therefore, we use 128 instead of the default
  // buffer pool size specified in `config.h`.
  try {
    buffer_pool_manager_ = new BufferPoolManagerInstance(128, disk_manager_, LRUK_REPLACER_K, log_manager_);
  } catch (NotImplementedException &e) {
    std::cerr << "BufferPoolManager is not implemented, only mock tables are supported." << std::endl;
    buffer_pool_manager_ = nullptr;
  }

  // Transaction (txn) related.
  lock_manager_ = new LockManager();
  txn_manager_ = new TransactionManager(lock_manager_, log_manager_);

  // Checkpoint related.
  checkpoint_manager_ = new CheckpointManager(txn_manager_, log_manager_, buffer_pool_manager_);

  // Catalog.
  catalog_ = new Catalog(buffer_pool_manager_, lock_manager_, log_manager_);

  // Execution engine.
  execution_engine_ = new ExecutionEngine(buffer_pool_manager_, txn_manager_, catalog_);
}

// 用于显示数据库中的所有表，它从目录中获取表名，并将表的 OID、名称和模式写入结果中
void BustubInstance::CmdDisplayTables(ResultWriter &writer) {
  auto table_names = catalog_->GetTableNames();
  writer.BeginTable(false);
  writer.BeginHeader();
  writer.WriteHeaderCell("oid");
  writer.WriteHeaderCell("name");
  writer.WriteHeaderCell("cols");
  writer.EndHeader();
  for (const auto &name : table_names) {
    writer.BeginRow();
    const auto *table_info = catalog_->GetTable(name);
    writer.WriteCell(fmt::format("{}", table_info->oid_));
    writer.WriteCell(table_info->name_);
    writer.WriteCell(table_info->schema_.ToString());
    writer.EndRow();
  }
  writer.EndTable();
}

// 用于显示数据库中的所有索引
// 从目录中获取表名，并将每个表的索引信息（如表名、索引 OID、索引名称和索引列）写入结果中
void BustubInstance::CmdDisplayIndices(ResultWriter &writer) {
  auto table_names = catalog_->GetTableNames();
  writer.BeginTable(false);
  writer.BeginHeader();
  writer.WriteHeaderCell("table_name");
  writer.WriteHeaderCell("index_oid");
  writer.WriteHeaderCell("index_name");
  writer.WriteHeaderCell("index_cols");
  writer.EndHeader();
  for (const auto &table_name : table_names) {
    for (const auto *index_info : catalog_->GetTableIndexes(table_name)) {
      writer.BeginRow();
      writer.WriteCell(table_name);
      writer.WriteCell(fmt::format("{}", index_info->index_oid_));
      writer.WriteCell(index_info->name_);
      writer.WriteCell(index_info->key_schema_.ToString());
      writer.EndRow();
    }
  }
  writer.EndTable();
}

// 用于将单个字符串写入结果中。它创建一个包含单行单元格的表，并将字符串写入该单元格
// 用于生成结构化的输出，例如生成 HTML 表格或其他格式的表格数据
void BustubInstance::WriteOneCell(const std::string &cell, ResultWriter &writer) {
  writer.BeginTable(true);
  writer.BeginRow();
  writer.WriteCell(cell);
  writer.EndRow();
  writer.EndTable();
}

// 用于显示帮助信息
void BustubInstance::CmdDisplayHelp(ResultWriter &writer) {
  std::string help = R"(Welcome to the BusTub shell!

\dt: show all tables
\di: show all indices
\help: show this message again

BusTub shell currently only supports a small set of Postgres queries. We'll set
up a doc describing the current status later. It will silently ignore some parts
of the query, so it's normal that you'll get a wrong result when executing
unsupported SQL queries. This shell will be able to run `create table` only
after you have completed the buffer pool manager. It will be able to execute SQL
queries after you have implemented necessary query executors. Use `explain` to
see the execution plan of your query.
)";
  WriteOneCell(help, writer);
}

// 用于执行 SQL 语句
auto BustubInstance::ExecuteSql(const std::string &sql, ResultWriter &writer) -> bool {
  auto txn = txn_manager_->Begin();
  auto result = ExecuteSqlTxn(sql, writer, txn);
  txn_manager_->Commit(txn);
  delete txn;
  return result;
}

auto BustubInstance::ExecuteSqlTxn(const std::string &sql, ResultWriter &writer, Transaction *txn) -> bool {
  // 首先检查 SQL 语句是否是内部命令（以 \ 开头），并调用相应的命令处理函数。
  if (!sql.empty() && sql[0] == '\\') {
    // Internal meta-commands, like in `psql`.
    if (sql == "\\dt") {
      CmdDisplayTables(writer);
      return true;
    }
    if (sql == "\\di") {
      CmdDisplayIndices(writer);
      return true;
    }
    if (sql == "\\help") {
      CmdDisplayHelp(writer);
      return true;
    }
    throw Exception(fmt::format("unsupported internal command: {}", sql));
  }

  bool is_successful = true;

  // 获取一个共享锁 catalog_lock_，以确保在解析和保存 SQL 语句时不会有其他线程修改目录
  std::shared_lock<std::shared_mutex> l(catalog_lock_);
  bustub::Binder binder(*catalog_);
  // 调用 binder 的 ParseAndSave 方法来解析并保存 SQL 语句
  binder.ParseAndSave(sql);
  // 释放共享锁
  l.unlock();

  // 如果不是内部命令，它会解析并绑定 SQL 语句
  // 然后根据语句类型执行相应的操作，如创建表、创建索引、显示变量、设置变量和解释查询等
  for (auto *stmt : binder.statement_nodes_) {
    // 每个语句都被绑定为一个 statement 对象，然后根据其类型执行相应的操作
    auto statement = binder.BindStatement(stmt);
    switch (statement->type_) {
      case StatementType::CREATE_STATEMENT: {  // 创建表
        const auto &create_stmt = dynamic_cast<const CreateStatement &>(*statement);
        // 使用 std::unique_lock 锁定了一个共享互斥锁 catalog_lock_
        // 以确保在创建表时不会有其他线程同时修改目录
        std::unique_lock<std::shared_mutex> l(catalog_lock_);
        // 调用 catalog_ 对象的 CreateTable 方法
        // 传入事务 txn、表名 create_stmt.table_ 和表的模式 Schema(create_stmt.columns_)
        auto info = catalog_->CreateTable(txn, create_stmt.table_, Schema(create_stmt.columns_));
        l.unlock();

        if (info == nullptr) {
          throw bustub::Exception("Failed to create table");
        }
        // 将结果写入到writer中
        WriteOneCell(fmt::format("Table created with id = {}", info->oid_), writer);
        continue;
      }
      case StatementType::INDEX_STATEMENT: {  // 创建索引
        // 通过 dynamic_cast 将通用的 statement 对象转换为具体的 IndexStatement 类型
        // 并将其引用存储在 index_stmt 变量中
        const auto &index_stmt = dynamic_cast<const IndexStatement &>(*statement);

        std::vector<uint32_t> col_ids;
        // 遍历 index_stmt.cols_ 中的每个列
        // 并通过调用 index_stmt.table_->schema_.GetColIdx(col->col_name_.back()) 获取列的索引
        // 并将其添加到 col_ids 向量中
        for (const auto &col : index_stmt.cols_) {
          auto idx = index_stmt.table_->schema_.GetColIdx(col->col_name_.back());
          col_ids.push_back(idx);
          // 检查每个列的类型是否为 TypeId::INTEGER
          // 如果不是，则抛出 NotImplementedException 异常，当前只支持在整数列上创建索引
          if (index_stmt.table_->schema_.GetColumn(idx).GetType() != TypeId::INTEGER) {
            throw NotImplementedException("only support creating index on integer column");
          }
        }
        // 检查 col_ids 的大小是否为 1
        // 如果不是，则抛出 NotImplementedException 异常，表示当前只支持在单列上创建索引
        if (col_ids.size() != 1) {
          throw NotImplementedException("only support creating index with exactly one column");
        }
        // 通过调用 Schema::CopySchema 方法，使用 col_ids 创建一个新的 key_schema
        // 该模式只包含索引涉及的列
        auto key_schema = Schema::CopySchema(&index_stmt.table_->schema_, col_ids);

        std::unique_lock<std::shared_mutex> l(catalog_lock_);
        // 调用 catalog_->CreateIndex 方法创建索引
        // 接受多个参数，包括事务对象 txn、索引名称 index_stmt.index_name_、
        // 表对象 index_stmt.table_->table_、表模式 index_stmt.table_->schema_、
        // 键模式 key_schema、列索引 col_ids、整数大小 INTEGER_SIZE 和哈希函数 IntegerHashFunctionType{}
        auto info = catalog_->CreateIndex<IntegerKeyType, IntegerValueType, IntegerComparatorType>(
            txn, index_stmt.index_name_, index_stmt.table_->table_, index_stmt.table_->schema_, key_schema, col_ids,
            INTEGER_SIZE, IntegerHashFunctionType{});
        l.unlock();

        if (info == nullptr) {
          throw bustub::Exception("Failed to create index");
        }
        // 将结果写入到writer中
        WriteOneCell(fmt::format("Index created with id = {}", info->index_oid_), writer);
        continue;
      }
      case StatementType::VARIABLE_SHOW_STATEMENT: {  // 显示变量
        const auto &show_stmt = dynamic_cast<const VariableShowStatement &>(*statement);
        auto content = GetSessionVariable(show_stmt.variable_);
        WriteOneCell(fmt::format("{}={}", show_stmt.variable_, content), writer);
        continue;
      }
      case StatementType::VARIABLE_SET_STATEMENT: {  // 设置变量
        const auto &set_stmt = dynamic_cast<const VariableSetStatement &>(*statement);
        session_variables_[set_stmt.variable_] = set_stmt.value_;
        continue;
      }
      case StatementType::EXPLAIN_STATEMENT: {  // 解释查询
        // 对于普通查询，它会进行查询计划和优化，然后执行查询并将结果写入结果中
        const auto &explain_stmt = dynamic_cast<const ExplainStatement &>(*statement);
        std::string output;

        // Print binder result.
        if ((explain_stmt.options_ & ExplainOptions::BINDER) != 0) {
          output += "=== BINDER ===";
          output += "\n";
          output += explain_stmt.statement_->ToString();
          output += "\n";
        }

        std::shared_lock<std::shared_mutex> l(catalog_lock_);

        bustub::Planner planner(*catalog_);
        planner.PlanQuery(*explain_stmt.statement_);

        bool show_schema = (explain_stmt.options_ & ExplainOptions::SCHEMA) != 0;

        // Print planner result.
        if ((explain_stmt.options_ & ExplainOptions::PLANNER) != 0) {
          output += "=== PLANNER ===";
          output += "\n";
          output += planner.plan_->ToString(show_schema);
          output += "\n";
        }

        // Print optimizer result.
        bustub::Optimizer optimizer(*catalog_, IsForceStarterRule());
        auto optimized_plan = optimizer.Optimize(planner.plan_);

        l.unlock();

        if ((explain_stmt.options_ & ExplainOptions::OPTIMIZER) != 0) {
          output += "=== OPTIMIZER ===";
          output += "\n";
          output += optimized_plan->ToString(show_schema);
          output += "\n";
        }

        WriteOneCell(output, writer);

        continue;
      }
      default:
        break;
    }
    // 如果不是创建表、创建索引、显示变量、设置变量和解释查询等操作
    // 则为查询语句，执行查询并将结果写入结果中
    std::shared_lock<std::shared_mutex> l(catalog_lock_);

    // Plan the query.
    bustub::Planner planner(*catalog_);
    planner.PlanQuery(*statement);

    // Optimize the query.
    bustub::Optimizer optimizer(*catalog_, IsForceStarterRule());
    auto optimized_plan = optimizer.Optimize(planner.plan_);

    l.unlock();

    // Execute the query.
    auto exec_ctx = MakeExecutorContext(txn);
    std::vector<Tuple> result_set{};
    is_successful &= execution_engine_->Execute(optimized_plan, &result_set, txn, exec_ctx.get());

    // Return the result set as a vector of string.
    // 获取查询计划的输出模式 schema
    auto schema = planner.plan_->OutputSchema();

    // Generate header for the result set.
    // 使用 writer 对象生成结果集的表头
    // 首先调用 BeginTable 和 BeginHeader 方法开始生成表格和表头
    writer.BeginTable(false);
    writer.BeginHeader();
    // 然后遍历模式中的每一列，调用 WriteHeaderCell 方法写入列名
    for (const auto &column : schema.GetColumns()) {
      writer.WriteHeaderCell(column.GetName());
    }
    // 最后调用 EndHeader 方法结束表头的生成
    writer.EndHeader();

    // Transforming result set into strings.
    // 将结果集转换为字符串并写入表格
    for (const auto &tuple : result_set) {
      // 调用 BeginRow 方法开始生成行，然后遍历结果集中的每一个元组
      writer.BeginRow();
      for (uint32_t i = 0; i < schema.GetColumnCount(); i++) {
        // 调用 WriteCell 方法写入每一列的值
        writer.WriteCell(tuple.GetValue(&schema, i).ToString());
      }
      // 调用 EndRow 方法结束行的生成
      writer.EndRow();
    }
    // 所有行生成完毕后，调用 EndTable 方法结束表格的生成
    writer.EndTable();
  }

  return is_successful;
}

/**
 * FOR TEST ONLY. Generate test tables in this BusTub instance.
 * It's used in the shell to predefine some tables, as we don't support
 * create / drop table and insert for now. Should remove it in the future.
 */
void BustubInstance::GenerateTestTable() {
  auto txn = txn_manager_->Begin();
  auto exec_ctx = MakeExecutorContext(txn);
  TableGenerator gen{exec_ctx.get()};

  std::shared_lock<std::shared_mutex> l(catalog_lock_);
  gen.GenerateTestTables();
  l.unlock();

  txn_manager_->Commit(txn);
  delete txn;
}

/**
 * FOR TEST ONLY. Generate test tables in this BusTub instance.
 * It's used in the shell to predefine some tables, as we don't support
 * create / drop table and insert for now. Should remove it in the future.
 */
void BustubInstance::GenerateMockTable() {
  // The actual content generated by mock scan executors are described in `mock_scan_executor.cpp`.
  auto txn = txn_manager_->Begin();

  std::shared_lock<std::shared_mutex> l(catalog_lock_);
  for (auto table_name = &mock_table_list[0]; *table_name != nullptr; table_name++) {
    catalog_->CreateTable(txn, *table_name, GetMockTableSchemaOf(*table_name), false);
  }
  l.unlock();

  txn_manager_->Commit(txn);
  delete txn;
}

BustubInstance::~BustubInstance() {
  if (enable_logging) {
    log_manager_->StopFlushThread();
  }
  delete execution_engine_;
  delete catalog_;
  delete checkpoint_manager_;
  delete log_manager_;
  delete buffer_pool_manager_;
  delete lock_manager_;
  delete txn_manager_;
  delete disk_manager_;
}

}  // namespace bustub
