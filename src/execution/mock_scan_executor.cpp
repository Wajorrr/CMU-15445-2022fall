//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// mock_scan_executor.cpp
//
// Identification: src/execution/mock_scan_executor.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/mock_scan_executor.h"
#include <algorithm>
#include <random>

#include "common/exception.h"
#include "common/util/string_util.h"
#include "execution/expressions/column_value_expression.h"
#include "type/type_id.h"
#include "type/value_factory.h"

namespace bustub {

// 这些常量定义了模拟数据
// 包括助教列表、助教办公时间、课程日期、课程布尔值和模拟表列表。这些数据用于模拟不同的表和其内容
static const char *ta_list_2022[] = {"amstqq",      "durovo",     "joyceliaoo", "karthik-ramanathan-3006",
                                     "kush789",     "lmwnshn",    "mkpjnx",     "skyzh",
                                     "thepinetree", "timlee0119", "yliang412"};

static const char *ta_oh_2022[] = {"Tuesday",   "Wednesday", "Monday",  "Wednesday", "Thursday", "Friday",
                                   "Wednesday", "Randomly",  "Tuesday", "Monday",    "Tuesday"};

static const char *course_on_date[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

static int course_on_bool[] = {0, 1, 0, 1, 0, 1, 1};

const char *mock_table_list[] = {"__mock_table_1", "__mock_table_2", "__mock_table_3", "__mock_table_tas_2022",
                                 "__mock_agg_input_small", "__mock_agg_input_big", "__mock_table_schedule_2022",
                                 "__mock_table_123", "__mock_graph",
                                 // For leaderboard Q1
                                 "__mock_t1_50k", "__mock_t2_100k", "__mock_t3_1k",
                                 // For leaderboard Q2
                                 "__mock_t4_1m", "__mock_t5_1m", "__mock_t6_1m",
                                 // For leaderboard Q3
                                 "__mock_t7", "__mock_t8", nullptr};

static const int GRAPH_NODE_CNT = 10;

// 根据传入的表名返回相应的表模式（Schema）
auto GetMockTableSchemaOf(const std::string &table) -> Schema {
  // 根据表名返回相应的模式
  // 如果表名不匹配，抛出异常
  if (table == "__mock_table_1") {
    return Schema{std::vector{{Column{"colA", TypeId::INTEGER}, {Column{"colB", TypeId::INTEGER}}}}};
  }

  if (table == "__mock_table_2") {
    return Schema{std::vector{Column{"colC", TypeId::VARCHAR, 128}, {Column{"colD", TypeId::VARCHAR, 128}}}};
  }

  if (table == "__mock_table_3") {
    return Schema{std::vector{Column{"colE", TypeId::INTEGER}, {Column{"colF", TypeId::VARCHAR, 128}}}};
  }

  if (table == "__mock_table_tas_2022") {
    return Schema{std::vector{Column{"github_id", TypeId::VARCHAR, 128}, Column{"office_hour", TypeId::VARCHAR, 128}}};
  }

  if (table == "__mock_table_schedule_2022") {
    return Schema{std::vector{Column{"day_of_week", TypeId::VARCHAR, 128}, Column{"has_lecture", TypeId::INTEGER}}};
  }

  if (table == "__mock_agg_input_small" || table == "__mock_agg_input_big") {
    return Schema{std::vector{Column{"v1", TypeId::INTEGER}, Column{"v2", TypeId::INTEGER},
                              Column{"v3", TypeId::INTEGER}, Column{"v4", TypeId::INTEGER},
                              Column{"v5", TypeId::INTEGER}, Column{"v6", TypeId::VARCHAR, 128}}};
  }

  if (table == "__mock_graph") {
    return Schema{std::vector{Column{"src", TypeId::INTEGER}, Column{"dst", TypeId::INTEGER},
                              Column{"src_label", TypeId::VARCHAR, 8}, Column{"dst_label", TypeId::VARCHAR, 8},
                              Column{"distance", TypeId::INTEGER}}};
  }

  if (table == "__mock_table_123") {
    return Schema{std::vector{Column{"number", TypeId::INTEGER}}};
  }

  if (table == "__mock_t1_50k" || table == "__mock_t2_100k" || table == "__mock_t3_1k" || table == "__mock_t4_1m" ||
      table == "__mock_t5_1m" || table == "__mock_t6_1m") {
    return Schema{std::vector{Column{"x", TypeId::INTEGER}, Column{"y", TypeId::INTEGER}}};
  }

  if (table == "__mock_t7") {
    return Schema{
        std::vector{Column{"v", TypeId::INTEGER}, Column{"v1", TypeId::INTEGER}, Column{"v2", TypeId::INTEGER}}};
  }

  if (table == "__mock_t8") {
    return Schema{std::vector{Column{"v4", TypeId::INTEGER}}};
  }

  throw bustub::Exception(fmt::format("mock table {} not found", table));
}

// 根据传入的计划节点返回相应的表大小
auto GetSizeOf(const MockScanPlanNode *plan) -> size_t {
  // 根据表名返回相应的表大小
  // 如果表名不匹配，返回 0
  const auto &table = plan->GetTable();

  if (table == "__mock_table_1") {
    return 100;
  }

  if (table == "__mock_table_2") {
    return 100;
  }

  if (table == "__mock_table_3") {
    return 100;
  }

  if (table == "__mock_table_tas_2022") {
    return sizeof(ta_list_2022) / sizeof(ta_list_2022[0]);
  }

  if (table == "__mock_table_schedule_2022") {
    return sizeof(course_on_date) / sizeof(course_on_date[0]);
  }

  if (table == "__mock_agg_input_small") {
    return 1000;
  }

  if (table == "__mock_agg_input_big") {
    return 10000;
  }

  if (table == "__mock_graph") {
    return GRAPH_NODE_CNT * GRAPH_NODE_CNT;
  }

  if (table == "__mock_table_123") {
    return 3;
  }

  if (table == "__mock_t1_50k") {
    return 50000;
  }

  if (table == "__mock_t2_100k") {
    return 100000;
  }

  if (table == "__mock_t3_1k") {
    return 1000;
  }

  if (table == "__mock_t4_1m" || table == "__mock_t5_1m" || table == "__mock_t6_1m") {
    return 1000000;
  }

  if (table == "__mock_t7") {
    return 1000000;
  }

  if (table == "__mock_t8") {
    return 10;
  }

  return 0;
}

// 根据传入的计划节点返回表是否需要打乱顺序
auto GetShuffled(const MockScanPlanNode *plan) -> bool {
  // 根据表名返回是否需要打乱
  const auto &table = plan->GetTable();

  if (table == "__mock_t1_50k") {
    return true;
  }

  if (table == "__mock_t2_100k") {
    return true;
  }

  if (table == "__mock_t3_1k") {
    return true;
  }

  return false;
}

// 根据传入的计划节点返回相应的生成元组的函数
// 如果表名不匹配任何已知的表，则返回一个默认的生成全零元组的函数
auto GetFunctionOf(const MockScanPlanNode *plan) -> std::function<Tuple(size_t)> {
  // 根据表名返回相应的生成元组的函数
  // 如果表名不匹配，返回一个默认的生成全零元组的函数
  const auto &table = plan->GetTable();

  if (table == "__mock_table_1") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.reserve(2);
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 100));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_table_2") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.reserve(2);
      values.push_back(ValueFactory::GetVarcharValue(fmt::format("{}-\U0001F4A9", cursor)));  // the poop emoji
      values.push_back(
          ValueFactory::GetVarcharValue(StringUtil::Repeat("\U0001F607", cursor % 8)));  // the innocent emoji
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_table_3") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.reserve(2);
      if (cursor % 2 == 0) {
        values.push_back(ValueFactory::GetIntegerValue(cursor));
      } else {
        values.push_back(ValueFactory::GetNullValueByType(TypeId::INTEGER));
      }
      values.push_back(ValueFactory::GetVarcharValue(fmt::format("{}-\U0001F4A9", cursor)));  // the poop emoji
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_table_tas_2022") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetVarcharValue(ta_list_2022[cursor]));
      values.push_back(ValueFactory::GetVarcharValue(ta_oh_2022[cursor]));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_table_schedule_2022") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetVarcharValue(course_on_date[cursor]));
      values.push_back(ValueFactory::GetIntegerValue(course_on_bool[cursor]));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_agg_input_small") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue((cursor + 2) % 10));
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue((cursor + 50) % 100));
      values.push_back(ValueFactory::GetIntegerValue(cursor / 100));
      values.push_back(ValueFactory::GetIntegerValue(233));
      values.push_back(
          ValueFactory::GetVarcharValue(StringUtil::Repeat("\U0001F4A9", (cursor % 8) + 1)));  // the poop emoji
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_agg_input_big") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue((cursor + 2) % 10));
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue((cursor + 50) % 100));
      values.push_back(ValueFactory::GetIntegerValue(cursor / 1000));
      values.push_back(ValueFactory::GetIntegerValue(233));
      values.push_back(
          ValueFactory::GetVarcharValue(StringUtil::Repeat("\U0001F4A9", (cursor % 16) + 1)));  // the poop emoji
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_table_123") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor + 1));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_graph") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      int src = cursor % GRAPH_NODE_CNT;
      int dst = cursor / GRAPH_NODE_CNT;
      values.push_back(ValueFactory::GetIntegerValue(src));
      values.push_back(ValueFactory::GetIntegerValue(dst));
      values.push_back(ValueFactory::GetVarcharValue(fmt::format("{:03}", src)));
      values.push_back(ValueFactory::GetVarcharValue(fmt::format("{:03}", dst)));
      if (src == dst) {
        values.push_back(ValueFactory::GetNullValueByType(TypeId::INTEGER));
      } else {
        values.push_back(ValueFactory::GetIntegerValue(1));
      }
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t1_50k") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor * 10));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 1000));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t2_100k") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 100));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t3_1k") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor * 100));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 10000));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t4_1m") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      cursor = cursor % 500000;
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 10));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t5_1m") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      cursor = (cursor + 30000) % 500000;
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 10));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t6_1m") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      cursor = (cursor + 60000) % 500000;
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor * 10));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t7") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor % 20));
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  if (table == "__mock_t8") {
    return [plan](size_t cursor) {
      std::vector<Value> values{};
      values.push_back(ValueFactory::GetIntegerValue(cursor));
      return Tuple{values, &plan->OutputSchema()};
    };
  }

  // By default, return table of all 0.
  return [plan](size_t cursor) {
    std::vector<Value> values{};
    values.reserve(plan->OutputSchema().GetColumnCount());
    for (const auto &column : plan->OutputSchema().GetColumns()) {
      values.push_back(ValueFactory::GetZeroValueByType(column.GetType()));
    }
    return Tuple{values, &plan->OutputSchema()};
  };
}

// 初始化 MockScanExecutor 对象
MockScanExecutor::MockScanExecutor(ExecutorContext *exec_ctx, const MockScanPlanNode *plan)
    : AbstractExecutor{exec_ctx}, plan_{plan}, func_(GetFunctionOf(plan)), size_(GetSizeOf(plan)) {
  // 调用 GetFunctionOf 获取生成元组的函数，调用 GetSizeOf 获取表的大小
  // 并根据 GetShuffled 的结果决定是否打乱索引
  if (GetShuffled(plan)) {
    for (size_t i = 0; i < size_; i++) {
      shuffled_idx_.push_back(i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(shuffled_idx_.begin(), shuffled_idx_.end(), g);
  }
}

void MockScanExecutor::Init() {
  // Reset the cursor
  cursor_ = 0;
}

// 用于获取下一个元组
auto MockScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (cursor_ == size_) {
    // Scan complete
    return EXECUTOR_EXHAUSTED;
  }
  // 根据是否打乱索引来生成元组，并将游标加 1，返回 EXECUTOR_ACTIVE
  if (shuffled_idx_.empty()) {
    *tuple = func_(cursor_);
  } else {
    *tuple = func_(shuffled_idx_[cursor_]);
  }
  ++cursor_;
  *rid = MakeDummyRID();
  return EXECUTOR_ACTIVE;
}

auto MockScanExecutor::MakeDummyRID() -> RID { return RID{0}; }

}  // namespace bustub
