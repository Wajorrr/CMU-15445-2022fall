//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// macros.h
//
// Identification: src/include/common/macros.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <stdexcept>

namespace bustub {

// 如果 expr 为 false，则程序会终止并输出 message
#define BUSTUB_ASSERT(expr, message) assert((expr) && (message))

// 用于标记尚未实现的功能
// 如果调用了未实现的功能，程序会抛出 std::logic_error 异常，并附带 message 信息
#define UNIMPLEMENTED(message) throw std::logic_error(message)

// 用于在运行时确保某个条件成立
// 如果 expr 为 false，则抛出 std::logic_error 异常，并附带 message 信息
#define BUSTUB_ENSURE(expr, message) \
  if (!(expr)) {                     \
    throw std::logic_error(message); \
  }

// 用于标记不应该被执行到的代码路径
// 如果执行到了这些代码，程序会抛出 std::logic_error 异常，并附带 message 信息
#define UNREACHABLE(message) throw std::logic_error(message)

// Macros to disable copying and moving
// 用于禁止类的复制操作，删除了类的复制构造函数和复制赋值运算符
// /* NOLINT */ 注释用于抑制代码检查工具的警告
#define DISALLOW_COPY(cname)                                    \
  cname(const cname &) = delete;                   /* NOLINT */ \
  auto operator=(const cname &)->cname & = delete; /* NOLINT */

// 用于禁止类的移动操作，删除了类的移动构造函数和移动赋值运算符
#define DISALLOW_MOVE(cname)                               \
  cname(cname &&) = delete;                   /* NOLINT */ \
  auto operator=(cname &&)->cname & = delete; /* NOLINT */

// 同时禁止类的复制和移动操作
#define DISALLOW_COPY_AND_MOVE(cname) \
  DISALLOW_COPY(cname);               \
  DISALLOW_MOVE(cname);

}  // namespace bustub
