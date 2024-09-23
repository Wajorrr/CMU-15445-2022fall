//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// execution_engine.h
//
// Identification: src/include/execution/execution_engine.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "concurrency/transaction.h"
#include "concurrency/transaction_manager.h"
#include "execution/executor_context.h"
#include "execution/executor_factory.h"
#include "execution/plans/abstract_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * The ExecutionEngine class executes query plans.
 */
// 用于执行查询计划，接收查询计划并执行它们，生成结果集
class ExecutionEngine {
 public:
  /**
   * Construct a new ExecutionEngine instance.
   * @param bpm The buffer pool manager used by the execution engine
   * @param txn_mgr The transaction manager used by the execution engine
   * @param catalog The catalog used by the execution engine
   */
  ExecutionEngine(BufferPoolManager *bpm, TransactionManager *txn_mgr, Catalog *catalog)
      : bpm_{bpm}, txn_mgr_{txn_mgr}, catalog_{catalog} {}

  DISALLOW_COPY_AND_MOVE(ExecutionEngine);

  /**
   * Execute a query plan.
   * @param plan The query plan to execute
   * @param result_set The set of tuples produced by executing the plan
   * @param txn The transaction context in which the query executes
   * @param exec_ctx The executor context in which the query executes
   * @return `true` if execution of the query plan succeeds, `false` otherwise
   */
  // NOLINTNEXTLINE
  // 用于执行查询计划
  // 接收四个参数：查询计划 plan、结果集 result_set、事务上下文 txn 和执行上下文 exec_ctx
  auto Execute(const AbstractPlanNodeRef &plan, std::vector<Tuple> *result_set, Transaction *txn,
               ExecutorContext *exec_ctx) -> bool {
    // 确保事务上下文与执行上下文中的事务一致
    BUSTUB_ASSERT((txn == exec_ctx->GetTransaction()), "Broken Invariant");

    // Construct the executor for the abstract plan node
    // 使用 ExecutorFactory 创建一个执行器
    auto executor = ExecutorFactory::CreateExecutor(exec_ctx, plan);

    // Initialize the executor
    auto executor_succeeded = true;

    try {
      // 初始化执行器
      executor->Init();
      // PollExecutor 方法是一个私有的静态方法
      // 用于从执行器中提取结果元组，直到执行器耗尽或发生异常
      // 接收三个参数：根执行器 executor、查询计划 plan 和结果集 result_set
      PollExecutor(executor.get(), plan, result_set);
    } catch (const ExecutionException &ex) {
#ifndef NDEBUG
      LOG_ERROR("Error Encountered in Executor Execution: %s", ex.what());
#endif
      executor_succeeded = false;
      if (result_set != nullptr) {
        result_set->clear();
      }
    }

    return executor_succeeded;
  }

 private:
  /**
   * Poll the executor until exhausted, or exception escapes.
   * @param executor The root executor
   * @param plan The plan to execute
   * @param result_set The tuple result set
   */
  // 用于从执行器中提取结果元组
  static void PollExecutor(AbstractExecutor *executor, const AbstractPlanNodeRef &plan,
                           std::vector<Tuple> *result_set) {
    RID rid{};
    Tuple tuple{};
    // 不断调用执行器的 Next 方法获取下一个元组，并将其添加到结果集中
    while (executor->Next(&tuple, &rid)) {
      if (result_set != nullptr) {
        result_set->push_back(tuple);
      }
    }
  }

  [[maybe_unused]] BufferPoolManager *bpm_;
  [[maybe_unused]] TransactionManager *txn_mgr_;
  [[maybe_unused]] Catalog *catalog_;
};

}  // namespace bustub
