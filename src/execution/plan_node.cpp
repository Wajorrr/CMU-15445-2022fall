#include <algorithm>
#include <memory>
#include <vector>
#include "binder/table_ref/bound_base_table_ref.h"
#include "catalog/catalog.h"
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "execution/plans/aggregation_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "execution/plans/seq_scan_plan.h"

namespace bustub {

// 用于推断顺序扫描计划节点的模式
auto SeqScanPlanNode::InferScanSchema(const BoundBaseTableRef &table) -> Schema {
  std::vector<Column> schema;
  // 遍历表的所有列，并为每一列创建一个新的 Column 对象
  // 列名格式化为 table_name.column_name 的形式
  for (const auto &column : table.schema_.GetColumns()) {
    auto col_name = fmt::format("{}.{}", table.GetBoundTableName(), column.GetName());
    schema.emplace_back(Column(col_name, column));
  }
  // 返回一个包含所有列的 Schema 对象
  return Schema(schema);
}

// 用于推断嵌套循环连接计划节点的模式
auto NestedLoopJoinPlanNode::InferJoinSchema(const AbstractPlanNode &left, const AbstractPlanNode &right) -> Schema {
  std::vector<Column> schema;
  // 遍历左表和右表的所有列，并将这些列添加到模式中
  for (const auto &column : left.OutputSchema().GetColumns()) {
    schema.emplace_back(column);
  }
  for (const auto &column : right.OutputSchema().GetColumns()) {
    schema.emplace_back(column);
  }
  // 返回一个包含所有列的 Schema 对象
  return Schema(schema);
}

// 用于推断投影计划节点的模式
auto ProjectionPlanNode::InferProjectionSchema(const std::vector<AbstractExpressionRef> &expressions) -> Schema {
  std::vector<Column> schema;
  // 遍历所有表达式，并根据表达式的返回类型创建新的 Column 对象
  for (const auto &expr : expressions) {
    auto type_id = expr->GetReturnType();
    // 如果返回类型是 VARCHAR，则使用默认长度
    if (type_id != TypeId::VARCHAR) {
      schema.emplace_back("<unnamed>", type_id);
    } else {
      // TODO(chi): infer the correct VARCHAR length. Maybe it doesn't matter for executors?
      schema.emplace_back("<unnamed>", type_id, VARCHAR_DEFAULT_LENGTH);
    }
  }
  // 返回一个包含所有列的 Schema 对象
  return Schema(schema);
}

// 重命名给定模式中的列
auto ProjectionPlanNode::RenameSchema(const Schema &schema, const std::vector<std::string> &col_names) -> Schema {
  std::vector<Column> output;
  // 首先检查 col_names 的大小是否与 schema 中的列数相同
  if (col_names.size() != schema.GetColumnCount()) {
    throw bustub::Exception("mismatched number of columns");
  }
  size_t idx = 0;
  // 遍历 schema 中的所有列
  // 并使用 col_names 中的对应名称创建新的 Column 对象，存储在 output 向量中
  for (const auto &column : schema.GetColumns()) {
    output.emplace_back(Column(col_names[idx++], column));
  }
  // 返回一个新的 Schema 对象，包含重命名后的列
  return Schema(output);
}

// 推断聚合计划节点的模式
auto AggregationPlanNode::InferAggSchema(const std::vector<AbstractExpressionRef> &group_bys,
                                         const std::vector<AbstractExpressionRef> &aggregates,
                                         const std::vector<AggregationType> &agg_types) -> Schema {
  std::vector<Column> output;
  // 预留足够的空间以容纳所有分组和聚合表达式
  output.reserve(group_bys.size() + aggregates.size());
  // 遍历 group_bys 向量，并根据每个表达式的返回类型创建新的 Column 对象
  for (const auto &column : group_bys) {
    // TODO(chi): correctly process VARCHAR column
    // 如果返回类型是 VARCHAR，则使用默认长度 128
    if (column->GetReturnType() == TypeId::VARCHAR) {
      output.emplace_back(Column("<unnamed>", column->GetReturnType(), 128));
    } else {
      output.emplace_back(Column("<unnamed>", column->GetReturnType()));
    }
  }
  // 遍历 aggregates 向量，并为每个聚合表达式创建一个新的 Column 对象，默认返回类型为 INTEGER
  for (size_t idx = 0; idx < aggregates.size(); idx++) {
    // TODO(chi): correctly infer agg call return type
    output.emplace_back(Column("<unnamed>", TypeId::INTEGER));
  }
  return Schema(output);
}

}  // namespace bustub
