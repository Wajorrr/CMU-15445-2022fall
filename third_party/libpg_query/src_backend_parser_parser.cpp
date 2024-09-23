/*--------------------------------------------------------------------
 * Symbols referenced in this file:
 * - raw_parser
 * - base_yylex
 * - raw_parser
 *--------------------------------------------------------------------
 */

/*-------------------------------------------------------------------------
 *
 * parser.c
 *		Main entry point/driver for PostgreSQL grammar
 *
 * Note that the grammar is not allowed to perform any table access
 * (since we need to be able to do basic parsing even while inside an
 * aborted transaction).  Therefore, the data structures returned by
 * the grammar are "raw" parsetrees that still need to be analyzed by
 * analyze.c and related files.
 *
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development PGGroup
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/parser/parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "pg_functions.hpp"

#include "parser/gramparse.hpp"
#include "parser/kwlist.hpp"
#include "parser/parser.hpp"

namespace duckdb_libpgquery {

/*
 * raw_parser
 *		Given a query in string form, do lexical and grammatical analysis.
 *
 * Returns a list of raw (un-analyzed) parse trees.  The immediate elements
 * of the list are always PGRawStmt nodes.
 */
// 三个变量：yyscanner、yyextra 和 yyresult
// yyscanner 是词法分析器的句柄
// yyextra 是一个结构体，用于存储词法分析器的额外状态信息
// yyresult 用于存储解析结果
PGList *raw_parser(const char *str) {
  core_yyscan_t yyscanner;
  base_yy_extra_type yyextra;
  int yyresult;

  // 初始化词法分析器 yyscanner
  // 并将输入字符串 str、核心词法分析器的额外状态 yyextra.core_yy_extra、
  // 扫描关键字 ScanKeywords 和关键字数量 NumScanKeywords 传递给它
  /* initialize the flex scanner */
  yyscanner = scanner_init(str, &yyextra.core_yy_extra, ScanKeywords, NumScanKeywords);

  // 设置 yyextra 的 have_lookahead 字段为 false，表示当前没有预读的 token
  /* base_yylex() only needs this much initialization */
  yyextra.have_lookahead = false;

  // 用 parser_init 初始化 Bison 解析器，并将 yyextra 传递给它
  /* initialize the bison parser */
  parser_init(&yyextra);

  // 调用 base_yyparse 开始解析过程，并将词法分析器 yyscanner 传递给它
  // 解析结果存储在 yyresult 中
  /* Parse! */
  yyresult = base_yyparse(yyscanner);

  // 调用 scanner_finish 清理词法分析器 yyscanner，释放其占用的内存
  /* Clean up (release memory) */
  scanner_finish(yyscanner);

  if (yyresult) /* error */
    return NIL;

  // 函数返回 yyextra.parsetree，即解析树列表
  return yyextra.parsetree;
}

bool is_keyword(const char *text) { return ScanKeywordLookup(text, ScanKeywords, NumScanKeywords) != NULL; }

// 返回一个包含所有 SQL 关键字的列表
// 该函数的返回类型是 std::vector<PGKeyword>，其中 PGKeyword 是一个结构体，用于存储关键字及其类别
std::vector<PGKeyword> keyword_list() {
  std::vector<PGKeyword> result;
  for (size_t i = 0; i < NumScanKeywords; i++) {
    PGKeyword keyword;
    keyword.text = ScanKeywords[i].name;
    switch (ScanKeywords[i].category) {
      case UNRESERVED_KEYWORD:  // 未保留关键字
        keyword.category = PGKeywordCategory::PG_KEYWORD_UNRESERVED;
        break;
      case RESERVED_KEYWORD:  // 保留关键字
        keyword.category = PGKeywordCategory::PG_KEYWORD_RESERVED;
        break;
      case TYPE_FUNC_NAME_KEYWORD:  // 类型或函数名称关键字
        keyword.category = PGKeywordCategory::PG_KEYWORD_TYPE_FUNC;
        break;
      case COL_NAME_KEYWORD:  // 列名称关键字
        keyword.category = PGKeywordCategory::PG_KEYWORD_COL_NAME;
        break;
    }
    result.push_back(keyword);
  }
  return result;
}

std::vector<PGSimplifiedToken> tokenize(const char *str) {
  core_yyscan_t yyscanner;
  base_yy_extra_type yyextra;

  std::vector<PGSimplifiedToken> result;
  yyscanner = scanner_init(str, &yyextra.core_yy_extra, ScanKeywords, NumScanKeywords);
  yyextra.have_lookahead = false;

  while (true) {
    YYSTYPE type;
    YYLTYPE loc;
    int token;
    try {
      token = base_yylex(&type, &loc, yyscanner);
    } catch (...) {
      token = 0;
    }
    if (token == 0) {
      break;
    }
    PGSimplifiedToken current_token;
    // 创建一个 PGSimplifiedToken 类型的变量 current_token
    // 并使用 switch 语句根据 token 的值设置 current_token.type
    switch (token) {
      case IDENT:  // 标识符
        current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_IDENTIFIER;
        break;
      case ICONST:
      case FCONST:  // 数值常量
        current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_NUMERIC_CONSTANT;
        break;
      case SCONST:
      case BCONST:
      case XCONST:  // 字符串常量
        current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_STRING_CONSTANT;
        break;
      case Op:
      case PARAM:
      case COLON_EQUALS:
      case EQUALS_GREATER:
      case LESS_EQUALS:
      case GREATER_EQUALS:
      case NOT_EQUALS:  // 操作符
        current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_OPERATOR;
        break;
      default:
        if (token >= 255) {
          // non-ascii value, probably a keyword
          current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_KEYWORD;
        } else {
          // ascii value, probably an operator
          current_token.type = PGSimplifiedTokenType::PG_SIMPLIFIED_TOKEN_OPERATOR;
        }
        break;
    }
    current_token.start = loc;
    result.push_back(current_token);
  }

  scanner_finish(yyscanner);
  return result;
}

/*
 * Intermediate filter between parser and core lexer (core_yylex in scan.l).
 *
 * This filter is needed because in some cases the standard SQL grammar
 * requires more than one token lookahead.  We reduce these cases to one-token
 * lookahead by replacing tokens here, in order to keep the grammar LALR(1).
 *
 * Using a filter is simpler than trying to recognize multiword tokens
 * directly in scan.l, because we'd have to allow for comments between the
 * words.  Furthermore it's not clear how to do that without re-introducing
 * scanner backtrack, which would cost more performance than this filter
 * layer does.
 *
 * The filter also provides a convenient place to translate between
 * the core_YYSTYPE and YYSTYPE representations (which are really the
 * same thing anyway, but notationally they're different).
 */
int base_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, core_yyscan_t yyscanner) {
  base_yy_extra_type *yyextra = pg_yyget_extra(yyscanner);
  int cur_token;
  int next_token;
  int cur_token_length;
  YYLTYPE cur_yylloc;

  /* Get next token --- we might already have it */
  if (yyextra->have_lookahead) {
    cur_token = yyextra->lookahead_token;
    lvalp->core_yystype = yyextra->lookahead_yylval;
    *llocp = yyextra->lookahead_yylloc;
    *(yyextra->lookahead_end) = yyextra->lookahead_hold_char;
    yyextra->have_lookahead = false;
  } else
    cur_token = core_yylex(&(lvalp->core_yystype), llocp, yyscanner);

  /*
   * If this token isn't one that requires lookahead, just return it.  If it
   * does, determine the token length.  (We could get that via strlen(), but
   * since we have such a small set of possibilities, hardwiring seems
   * feasible and more efficient.)
   */
  switch (cur_token) {
    case NOT:
      cur_token_length = 3;
      break;
    case NULLS_P:
      cur_token_length = 5;
      break;
    case WITH:
      cur_token_length = 4;
      break;
    default:
      return cur_token;
  }

  /*
   * Identify end+1 of current token.  core_yylex() has temporarily stored a
   * '\0' here, and will undo that when we call it again.  We need to redo
   * it to fully revert the lookahead call for error reporting purposes.
   */
  yyextra->lookahead_end = yyextra->core_yy_extra.scanbuf + *llocp + cur_token_length;
  Assert(*(yyextra->lookahead_end) == '\0');

  /*
   * Save and restore *llocp around the call.  It might look like we could
   * avoid this by just passing &lookahead_yylloc to core_yylex(), but that
   * does not work because flex actually holds onto the last-passed pointer
   * internally, and will use that for error reporting.  We need any error
   * reports to point to the current token, not the next one.
   */
  cur_yylloc = *llocp;

  /* Get next token, saving outputs into lookahead variables */
  next_token = core_yylex(&(yyextra->lookahead_yylval), llocp, yyscanner);
  yyextra->lookahead_token = next_token;
  yyextra->lookahead_yylloc = *llocp;

  *llocp = cur_yylloc;

  /* Now revert the un-truncation of the current token */
  yyextra->lookahead_hold_char = *(yyextra->lookahead_end);
  *(yyextra->lookahead_end) = '\0';

  yyextra->have_lookahead = true;

  /* Replace cur_token if needed, based on lookahead */
  switch (cur_token) {
    case NOT:
      /* Replace NOT by NOT_LA if it's followed by BETWEEN, IN, etc */
      switch (next_token) {
        case BETWEEN:
        case IN_P:
        case LIKE:
        case ILIKE:
        case SIMILAR:
          cur_token = NOT_LA;
          break;
      }
      break;

    case NULLS_P:
      /* Replace NULLS_P by NULLS_LA if it's followed by FIRST or LAST */
      switch (next_token) {
        case FIRST_P:
        case LAST_P:
          cur_token = NULLS_LA;
          break;
      }
      break;

    case WITH:
      /* Replace WITH by WITH_LA if it's followed by TIME or ORDINALITY */
      switch (next_token) {
        case TIME:
        case ORDINALITY:
          cur_token = WITH_LA;
          break;
      }
      break;
  }

  return cur_token;
}

}  // namespace duckdb_libpgquery