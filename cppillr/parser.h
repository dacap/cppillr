// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include "cppillr/keywords.h"
#include "cppillr/lexer.h"

enum class NodeKind {
  ParamNode,
  ParamsNode,
  Expr,
  UnaryExpr,
  BinExpr,
  Literal,
  Return,
  CompoundStmt,
  Body,
  Function,
};

struct Node {
  NodeKind kind;

  Node(NodeKind kind) : kind(kind) { }
  virtual ~Node() { }
};

struct ParamNode : public Node {
  Keyword builtin_type;
  std::string name;
  ParamNode() : Node(NodeKind::ParamNode) { }
};

struct ParamsNode : public Node {
  std::vector<ParamNode*> params;
  ParamsNode() : Node(NodeKind::ParamsNode) { }
};

struct Expr : public Node {
  Expr(NodeKind kind = NodeKind::Expr) : Node(kind) { }
};

struct UnaryExpr : public Expr {
  char op;
  Expr* operand;
  UnaryExpr() : Expr(NodeKind::UnaryExpr) { }
  ~UnaryExpr() { delete operand; }
};

struct BinExpr : public Expr {
  char op;
  Expr* lhs;
  Expr* rhs;
  BinExpr() : Expr(NodeKind::BinExpr) { }
  ~BinExpr() {
    delete lhs;
    delete rhs;
  }
};

struct Literal : public Expr {
  int value;
  Literal() : Expr(NodeKind::Literal) { }
};

struct Stmt : public Node {
  Stmt(NodeKind kind) : Node(kind) { }
};

struct Return : public Stmt {
  Expr* expr = nullptr;
  Return() : Stmt(NodeKind::Return) { }
  ~Return() { delete expr; }
};

struct CompoundStmt : public Stmt {
  std::vector<Stmt*> stmts;
  CompoundStmt() : Stmt(NodeKind::CompoundStmt) { }
  ~CompoundStmt() {
    for (Stmt* s : stmts)
      delete s;
  }
};

struct BodyNode : public Node {
  // Tokens to be processed in the future.
  int lex_i;
  int beg_tok, end_tok;
  CompoundStmt* block = nullptr;
  BodyNode() : Node(NodeKind::Body) { }
  ~BodyNode() {
    delete block;
  }
};

struct FunctionNode : public Node {
  // Function
  Keyword builtin_type;
  std::string name;
  ParamsNode* params = nullptr;
  BodyNode* body = nullptr;
  FunctionNode() : Node(NodeKind::Function) { }
  ~FunctionNode() {
    delete body;
  }
};

struct ParserData {
  const LexData* lex;
  std::string fn;
  std::vector<FunctionNode*> functions;

  ParserData() { }
  ~ParserData() {
    for (FunctionNode* f : functions)
      delete f;
  }

  ParserData(ParserData&&) = default;
  ParserData(const ParserData&) = delete;
  ParserData& operator=(const ParserData&) = delete;
};

class Parser {
public:
  Parser(int lex_i = 0) : lex_i(lex_i) { }
  void parse(const LexData& lex);
  void parse_function_body(const LexData& lex, FunctionNode* f);

  ParserData&& move_data() { return std::move(data); }

private:
  const Token& goto_token(int i) {
    tok_i = i;
    if (i >= 0 && i < lex_data->tokens.size())
      tok = &lex_data->tokens[i];
    else
      tok = &eof;
    return *tok;
  }

  const Token& next_token() {
    goto_token(++tok_i);
    return *tok;
  }

  bool is(TokenKind kind) const {
    return (tok->kind == kind);
  }

  bool is_punctuator(char chr) const {
    return (tok->kind == TokenKind::Punctuator &&
            tok->i == chr);
  }

  bool is_builtin_type() const {
    return (tok->kind == TokenKind::Keyword &&
            (tok->i == key_auto ||
             tok->i == key_bool ||
             tok->i == key_char ||
             tok->i == key_char8_t ||
             tok->i == key_char16_t ||
             tok->i == key_char32_t ||
             tok->i == key_double ||
             tok->i == key_float ||
             tok->i == key_int ||
             tok->i == key_long ||
             tok->i == key_short ||
             tok->i == key_signed ||
             tok->i == key_unsigned ||
             tok->i == key_void ||
             tok->i == key_wchar_t));
  }

  void expect(TokenKind kind, const char* err) {
    if (next_token().kind != kind)
      error(err);
  }

  void expect(char chr) {
    next_token();
    if (!is_punctuator(chr)) {
      error("expecting '%c'", chr);
    }
  }

  bool dcl_seq();
  bool dcl();
  FunctionNode* function_definition();
  ParamsNode* function_params();
  BodyNode* function_body_fast();
  CompoundStmt* compound_statement();
  Stmt* statement();
  Return* return_stmt();
  Expr* expression();
  Expr* additive_expression();
  Expr* multiplicative_expression();
  Expr* primary_expression();
  bool pp_line();

  template<typename ...Args>
  void error(Args&& ...args) {
    char buf[4096];
    std::sprintf(buf, std::forward<Args>(args)...);
    if (tok) {
      std::printf("%s:%d:%d: %s\n",
                  lex_data->fn.c_str(),
                  tok->pos.line,
                  tok->pos.col,
                  buf);
    }
    else {
      std::printf("%s: %s\n",
                  lex_data->fn.c_str(),
                  buf);
    }
    std::exit(1);
  }

  ParserData data;
  int lex_i;
  int tok_i;
  Token eof = { TokenKind::Eof, TextPos(0, 0) };
  const LexData* lex_data;
  const Token* tok;
  int depth = 0;
};
