//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// catalog.h
//
// Identification: src/include/catalog/catalog.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "container/hash/hash_function.h"
#include "storage/index/b_plus_tree_index.h"
#include "storage/index/extendible_hash_table_index.h"
#include "storage/index/index.h"
#include "storage/table/table_heap.h"

namespace bustub {

/**
 * Typedefs
 */
using table_oid_t = uint32_t;
using column_oid_t = uint32_t;
using index_oid_t = uint32_t;

/**
 * The TableInfo class maintains metadata about a table.
 */
struct TableInfo {
  /**
   * Construct a new TableInfo instance.
   * @param schema The table schema
   * @param name The table name
   * @param table An owning pointer to the table heap
   * @param oid The unique OID for the table
   */
  TableInfo(Schema schema, std::string name, std::unique_ptr<TableHeap> &&table, table_oid_t oid)
      : schema_{std::move(schema)}, name_{std::move(name)}, table_{std::move(table)}, oid_{oid} {}
  /** The table schema */
  Schema schema_;  // 表的模式
  /** The table name */
  const std::string name_;  // 表的名称
  /** An owning pointer to the table heap */
  std::unique_ptr<TableHeap> table_;  // 指向表堆（TableHeap）的拥有指针
  /** The table OID */
  const table_oid_t oid_;  // 表的唯一标识符（OID）
};

/**
 * The IndexInfo class maintains metadata about a index.
 */
struct IndexInfo {
  /**
   * Construct a new IndexInfo instance.
   * @param key_schema The schema for the index key
   * @param name The name of the index
   * @param index An owning pointer to the index
   * @param index_oid The unique OID for the index
   * @param table_name The name of the table on which the index is created
   * @param key_size The size of the index key, in bytes
   */
  IndexInfo(Schema key_schema, std::string name, std::unique_ptr<Index> &&index, index_oid_t index_oid,
            std::string table_name, size_t key_size)
      : key_schema_{std::move(key_schema)},
        name_{std::move(name)},
        index_{std::move(index)},
        index_oid_{index_oid},
        table_name_{std::move(table_name)},
        key_size_{key_size} {}
  /** The schema for the index key */
  Schema key_schema_;  // 索引键的模式
  /** The name of the index */
  std::string name_;  // 索引的名称
  /** An owning pointer to the index */
  std::unique_ptr<Index> index_;  // 指向索引（Index）的拥有指针
  /** The unique OID for the index */
  index_oid_t index_oid_;  // 索引的唯一标识符
  /** The name of the table on which the index is created */
  std::string table_name_;  // 创建索引的表的名称
  /** The size of the index key, in bytes */
  const size_t key_size_;  // 索引键的大小，以字节为单位
};

/**
 * The Catalog is a non-persistent catalog that is designed for
 * use by executors within the DBMS execution engine. It handles
 * table creation, table lookup, index creation, and index lookup.
 */
// Catalog 类在 BusTub 数据库系统中用于管理数据库的元数据，包括表和索引的元数据
// 它提供了创建、获取和管理表和索引的方法
class Catalog {
 public:
  // 两个静态常量指针 NULL_TABLE_INFO 和 NULL_INDEX_INFO
  // 分别用于指示返回 TableInfo* 和 IndexInfo* 类型的操作失败时的情况
  /** Indicates that an operation returning a `TableInfo*` failed */
  static constexpr TableInfo *NULL_TABLE_INFO{nullptr};

  /** Indicates that an operation returning a `IndexInfo*` failed */
  static constexpr IndexInfo *NULL_INDEX_INFO{nullptr};

  /**
   * Construct a new Catalog instance.
   * @param bpm The buffer pool manager backing tables created by this catalog
   * @param lock_manager The lock manager in use by the system
   * @param log_manager The log manager in use by the system
   */
  Catalog(BufferPoolManager *bpm, LockManager *lock_manager, LogManager *log_manager)
      : bpm_{bpm}, lock_manager_{lock_manager}, log_manager_{log_manager} {}

  /**
   * Create a new table and return its metadata.
   * @param txn The transaction in which the table is being created
   * @param table_name The name of the new table, note that all tables beginning with `__` are reserved for the system.
   * @param schema The schema of the new table
   * @param create_table_heap whether to create a table heap for the new table
   * @return A (non-owning) pointer to the metadata for the table
   */
  // 创建一个新表，table_name: 表的名称、schema: 表的模式、txn: 当前事务
  // 布尔值 create_table_heap用于指示是否为新表创建表堆
  // 指向新创建表的元数据的指针
  auto CreateTable(Transaction *txn, const std::string &table_name, const Schema &schema, bool create_table_heap = true)
      -> TableInfo * {
    // 首先检查 table_names_ 容器中是否已经存在同名的表
    if (table_names_.count(table_name) != 0) {
      // 如果存在，则返回一个特殊的空表信息指针 NULL_TABLE_INFO，表示表创建失败
      return NULL_TABLE_INFO;
    }

    // Construct the table heap
    // 创建一个新的表堆
    std::unique_ptr<TableHeap> table = nullptr;

    // TODO(Wan,chi): This should be refactored into a private ctor for the binder tests, we shouldn't allow nullptr.
    // When create_table_heap == false, it means that we're running binder tests (where no txn will be provided) or
    // we are running shell without buffer pool. We don't need to create TableHeap in this case.
    if (create_table_heap) {
      table = std::make_unique<TableHeap>(bpm_, lock_manager_, log_manager_, txn);
    }

    // Fetch the table OID for the new table
    // 从一个原子变量 next_table_oid_ 中获取当前值，并将其递增1
    // 然后将获取到的值赋给 table_oid 变量
    const auto table_oid = next_table_oid_.fetch_add(1);

    // Construct the table information
    // 创建一个新的表元数据
    auto meta = std::make_unique<TableInfo>(schema, table_name, std::move(table), table_oid);
    auto *tmp = meta.get();

    // Update the internal tracking mechanisms
    // 创建oid到表元数据的映射
    tables_.emplace(table_oid, std::move(meta));
    // 创建表名到表标识符的映射
    table_names_.emplace(table_name, table_oid);
    // 创建表名到其索引的映射
    index_names_.emplace(table_name, std::unordered_map<std::string, index_oid_t>{});

    return tmp;
  }

  /**
   * Query table metadata by name.
   * @param table_name The name of the table
   * @return A (non-owning) pointer to the metadata for the table
   */
  // 根据表名获取表的元数据
  auto GetTable(const std::string &table_name) const -> TableInfo * {
    // 先根据表名查找表的标识符
    auto table_oid = table_names_.find(table_name);
    if (table_oid == table_names_.end()) {
      // Table not found
      return NULL_TABLE_INFO;
    }
    // 根据表的标识符获取表的元数据
    auto meta = tables_.find(table_oid->second);
    BUSTUB_ASSERT(meta != tables_.end(), "Broken Invariant");

    return (meta->second).get();
  }

  /**
   * Query table metadata by OID
   * @param table_oid The OID of the table to query
   * @return A (non-owning) pointer to the metadata for the table
   */
  // 根据表的id获取表的元数据
  auto GetTable(table_oid_t table_oid) const -> TableInfo * {
    auto meta = tables_.find(table_oid);
    if (meta == tables_.end()) {
      return NULL_TABLE_INFO;
    }

    return (meta->second).get();
  }

  /**
   * Create a new index, populate existing data of the table and return its metadata.
   * @param txn The transaction in which the table is being created
   * @param index_name The name of the new index
   * @param table_name The name of the table
   * @param schema The schema of the table
   * @param key_schema The schema of the key
   * @param key_attrs Key attributes
   * @param keysize Size of the key
   * @param hash_function The hash function for the index
   * @return A (non-owning) pointer to the metadata of the new table
   */
  // 创建一个新索引
  // txn: 当前事务、index_name: 索引的名称、table_name: 表的名称
  // schema: 索引的模式、key_schema: 索引键的模式、key_attrs: 索引键的属性
  // keysize: 索引键的大小、hash_function: 索引的哈希函数
  // 指向新创建索引的元数据的指针
  template <class KeyType, class ValueType, class KeyComparator>
  auto CreateIndex(Transaction *txn, const std::string &index_name, const std::string &table_name, const Schema &schema,
                   const Schema &key_schema, const std::vector<uint32_t> &key_attrs, std::size_t keysize,
                   HashFunction<KeyType> hash_function) -> IndexInfo * {
    // Reject the creation request for nonexistent table
    // 检查表 table_name 是否存在于 table_names_ 集合中
    if (table_names_.find(table_name) == table_names_.end()) {
      return NULL_INDEX_INFO;
    }

    // If the table exists, an entry for the table should already be present in index_names_
    BUSTUB_ASSERT((index_names_.find(table_name) != index_names_.end()), "Broken Invariant");

    // Determine if the requested index already exists for this table
    // 检查请求的索引 index_name 是否已经存在于该表的索引集合 table_indexes 中
    // 如果索引已经存在，函数同样返回 NULL_INDEX_INFO
    auto &table_indexes = index_names_.find(table_name)->second;
    if (table_indexes.find(index_name) != table_indexes.end()) {
      // The requested index already exists for this table
      return NULL_INDEX_INFO;
    }

    // Construct index metdata
    // 创建一个 IndexMetadata 对象 meta，包含索引的元数据，如索引名称、表名称、表模式和键属性
    auto meta = std::make_unique<IndexMetadata>(index_name, table_name, &schema, key_attrs);

    // Construct the index, take ownership of metadata
    // TODO(Kyle): We should update the API for CreateIndex
    // to allow specification of the index type itself, not
    // just the key, value, and comparator types

    // TODO(chi): support both hash index and btree index
    // 使用 meta 创建一个 BPlusTreeIndex 对象 index，并将元数据的所有权转移给 index
    auto index = std::make_unique<BPlusTreeIndex<KeyType, ValueType, KeyComparator>>(std::move(meta), bpm_);

    // Populate the index with all tuples in table heap
    // 从表中获取所有元组，并将它们插入到新创建的索引中
    auto *table_meta = GetTable(table_name);
    // 通过调用 GetTable 获取表的元数据 table_meta
    // 然后遍历表堆 heap 中的所有元组，将每个元组的键插入到索引中
    auto *heap = table_meta->table_.get();
    for (auto tuple = heap->Begin(txn); tuple != heap->End(); ++tuple) {
      index->InsertEntry(tuple->KeyFromTuple(schema, key_schema, key_attrs), tuple->GetRid(), txn);
    }

    // Get the next OID for the new index
    // 获取新索引的下一个 OID
    const auto index_oid = next_index_oid_.fetch_add(1);

    // Construct index information; IndexInfo takes ownership of the Index itself
    // 创建一个新的索引信息对象 index_info
    // 包含索引的数据、索引名称、索引对象、索引标识符、表名称和键大小
    auto index_info =
        std::make_unique<IndexInfo>(key_schema, index_name, std::move(index), index_oid, table_name, keysize);
    auto *tmp = index_info.get();

    // Update internal tracking
    // 索引标识符到索引信息的映射
    indexes_.emplace(index_oid, std::move(index_info));
    // 索引名称到索引标识符的映射
    table_indexes.emplace(index_name, index_oid);

    return tmp;
  }

  /**
   * Get the index `index_name` for table `table_name`.
   * @param index_name The name of the index for which to query
   * @param table_name The name of the table on which to perform query
   * @return A (non-owning) pointer to the metadata for the index
   */
  // 根据索引名和表名获取索引的元数据
  auto GetIndex(const std::string &index_name, const std::string &table_name) -> IndexInfo * {
    auto table = index_names_.find(table_name);
    if (table == index_names_.end()) {
      BUSTUB_ASSERT((table_names_.find(table_name) == table_names_.end()), "Broken Invariant");
      return NULL_INDEX_INFO;
    }

    auto &table_indexes = table->second;

    auto index_meta = table_indexes.find(index_name);
    if (index_meta == table_indexes.end()) {
      return NULL_INDEX_INFO;
    }

    auto index = indexes_.find(index_meta->second);
    BUSTUB_ASSERT((index != indexes_.end()), "Broken Invariant");

    return index->second.get();
  }

  /**
   * Get the index `index_name` for table identified by `table_oid`.
   * @param index_name The name of the index for which to query
   * @param table_oid The OID of the table on which to perform query
   * @return A (non-owning) pointer to the metadata for the index
   */
  // 根据索引名和表标识符获取索引的元数据
  auto GetIndex(const std::string &index_name, const table_oid_t table_oid) -> IndexInfo * {
    // Locate the table metadata for the specified table OID
    auto table_meta = tables_.find(table_oid);
    if (table_meta == tables_.end()) {
      // Table not found
      return NULL_INDEX_INFO;
    }

    return GetIndex(index_name, table_meta->second->name_);
  }

  /**
   * Get the index identifier by index OID.
   * @param index_oid The OID of the index for which to query
   * @return A (non-owning) pointer to the metadata for the index
   */
  // 根据索引标识符获取索引的元数据
  auto GetIndex(index_oid_t index_oid) -> IndexInfo * {
    auto index = indexes_.find(index_oid);
    if (index == indexes_.end()) {
      return NULL_INDEX_INFO;
    }

    return index->second.get();
  }

  /**
   * Get all of the indexes for the table identified by `table_name`.
   * @param table_name The name of the table for which indexes should be retrieved
   * @return A vector of IndexInfo* for each index on the given table, empty vector
   * in the event that the table exists but no indexes have been created for it
   */
  // 根据表名获取表的所有索引
  auto GetTableIndexes(const std::string &table_name) const -> std::vector<IndexInfo *> {
    // Ensure the table exists
    if (table_names_.find(table_name) == table_names_.end()) {
      return std::vector<IndexInfo *>{};
    }

    auto table_indexes = index_names_.find(table_name);
    BUSTUB_ASSERT((table_indexes != index_names_.end()), "Broken Invariant");

    std::vector<IndexInfo *> indexes{};
    indexes.reserve(table_indexes->second.size());
    for (const auto &index_meta : table_indexes->second) {
      auto index = indexes_.find(index_meta.second);
      BUSTUB_ASSERT((index != indexes_.end()), "Broken Invariant");
      indexes.push_back(index->second.get());
    }

    return indexes;
  }

  // 获取所有表的名称
  auto GetTableNames() -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto &x : table_names_) {
      result.push_back(x.first);
    }
    return result;
  }

 private:
  // 指向缓冲池管理器的指针，用于管理数据库页面的内存
  [[maybe_unused]] BufferPoolManager *bpm_;
  // 指向锁管理器的指针，用于管理事务的锁
  [[maybe_unused]] LockManager *lock_manager_;
  // 指向日志管理器的指针，用于管理数据库的日志记录
  [[maybe_unused]] LogManager *log_manager_;

  /**
   * Map table identifier -> table metadata.
   *
   * NOTE: `tables_` owns all table metadata.
   */
  // 存储表标识符到表元数据的映射。tables_ 拥有所有表元数据的唯一所有权
  std::unordered_map<table_oid_t, std::unique_ptr<TableInfo>> tables_;

  /** Map table name -> table identifiers. */
  // 存储表名到表标识符的映射
  std::unordered_map<std::string, table_oid_t> table_names_;

  /** The next table identifier to be used. */
  // 用于生成下一个表标识符的原子变量
  std::atomic<table_oid_t> next_table_oid_{0};

  /**
   * Map index identifier -> index metadata.
   *
   * NOTE: that `indexes_` owns all index metadata.
   */
  // 存储索引标识符到索引元数据的映射
  // indexes_ 拥有所有索引元数据的唯一所有权
  std::unordered_map<index_oid_t, std::unique_ptr<IndexInfo>> indexes_;

  /** Map table name -> index names -> index identifiers. */
  // 存储表名到索引名到索引标识符的映射
  std::unordered_map<std::string, std::unordered_map<std::string, index_oid_t>> index_names_;

  /** The next index identifier to be used. */
  // 用于生成下一个索引标识符的原子变量
  std::atomic<index_oid_t> next_index_oid_{0};
};

}  // namespace bustub
