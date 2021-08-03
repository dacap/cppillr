// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "cppillr/parser.h"

#include <memory>

void Parser::parse(const LexData& lex)
{
  data.fn = lex.fn;
  lex_data = &lex;
  goto_token(-1);
  dcl_seq();
}

// Converts a function body that was "fast parsed" (only tokens) into
// AST nodes.
void Parser::parse_function_body(const LexData& lex, FunctionNode* f)
{
  data.fn = lex.fn;
  lex_data = &lex;
  goto_token(f->body->beg_tok);
  f->body->block = compound_statement();
}

bool Parser::dcl_seq()
{
  while (next_token().kind != TokenKind::Eof) {
    if (!dcl())
      return false;
  }
  return true;
}

bool Parser::dcl()
{
  if (is_builtin_type()) {
    FunctionNode* f = function_definition();
    if (f) {
      data.functions.push_back(f);
      return true;
    }
  }

  return false;
}

FunctionNode* Parser::function_definition()
{
  if (is_builtin_type()) {
    auto f = std::make_unique<FunctionNode>();
    f->builtin_type = (Keyword)tok->i;

    expect(TokenKind::Identifier, "expecting identifier for function");
    f->name = lex_data->id_text(*tok);

    f->params = function_params();
    if (!f->params)
      return nullptr;

    f->body = function_body_fast();
    if (!f->body)
      return nullptr;

    return f.release();
  }

  return nullptr;
}

ParamsNode* Parser::function_params()
{
  auto ps = std::make_unique<ParamsNode>();

  expect('(');
  while (next_token().kind != TokenKind::Eof) {
    if (is_punctuator(')')) {
      return ps.release();
    }
    else if (is_builtin_type()) {
      auto p = std::make_unique<ParamNode>();
      p->builtin_type = (Keyword)tok->i;

      next_token();

      // Pointers
      while (is_punctuator('*')) // TODO add this info to param type
        next_token();

      if (is(TokenKind::Identifier)) { // Param with name
        p->name = lex_data->id_text(*tok);

        next_token();
        if (is_punctuator(')')) {
          ps->params.push_back(p.get());
          p.release();
          return ps.release();
        }
        else if (!is_punctuator(','))
          error("expecting ',' or ')' after param name");
      }
      // Param without name
      else if (is_punctuator(')')) {
        ps->params.push_back(p.get());
        p.release();
        return ps.release();
      }
      else if (!is_punctuator(','))
        error("expecting ',', ')', or param name after param type");

      ps->params.push_back(p.get());
      p.release();
    }
    else {
      error("expecting ')' or type");
    }
  }

  return nullptr;
}

BodyNode* Parser::function_body_fast()
{
  auto b = std::make_unique<BodyNode>();
  int scope = 0;

  expect('{');
  b->lex_i = lex_i;
  b->beg_tok = tok_i;
  while (next_token().kind != TokenKind::Eof) {
    if (is_punctuator('}')) {
      if (scope == 0) {
        b->end_tok = tok_i;
        return b.release();
      }
      else
        --scope;
    }
    else if (is_punctuator('{')) {
      ++scope;
    }
  }

  error("expecting '}' before EOF");
  return nullptr;
}

CompoundStmt* Parser::compound_statement()
{
  auto b = std::make_unique<CompoundStmt>();

  if (!is_punctuator('{'))
    error("expecting '{' to start a block");

  next_token();                 // Skip '{'

  while (tok->kind != TokenKind::Eof) {
    if (is_punctuator('}')) {
      return b.release();
    }
    else {
      auto s = statement();
      if (s)
        b->stmts.push_back(s);
      else
        return nullptr;
    }
  }

  error("expecting '}' before EOF");
  return nullptr;
}

Stmt* Parser::statement()
{
  if (is_punctuator(';')) {
    next_token(); // Skip ';', empty expression
    return nullptr;
  }
  else if (tok->kind == TokenKind::Keyword) {
    // Return statement
    switch (tok->i) {

      case key_return:
        return return_stmt();

      default:
        error("not supported keyword %s",
              keywords_id[tok->i].c_str());
        break;
    }
  }

  error("expecting '}' or statement");
  return nullptr;
}

Return* Parser::return_stmt()
{
  auto r = std::make_unique<Return>();
  bool err = false;

  if (next_token().kind != TokenKind::Eof) {
    if (!is_punctuator(';')) {
      r->expr = expression();
      if (!r->expr)
        err = true;
    }
    if (is_punctuator(';')) {
      next_token(); // Skip ';', return with expression
    }
  }
  else
    err = true;

  if (err)
    error("expecting ';' or expression for return statement");

  return r.release();
}

Expr* Parser::expression()
{
  return additive_expression();
}

// [expr.add]
Expr* Parser::additive_expression()
{
  std::unique_ptr<Expr> e(multiplicative_expression());
  if (!e)
    return nullptr;

  while (is_punctuator('+') ||
         is_punctuator('-')) {
    auto be = std::make_unique<BinExpr>();
    be->op = tok->i;
    be->lhs = e.release();

    next_token();
    be->rhs = multiplicative_expression();
    e = std::move(be);
  }

  return e.release();
}

// [expr.mul]
Expr* Parser::multiplicative_expression()
{
  std::unique_ptr<Expr> e(primary_expression());
  if (!e)
    return nullptr;

  while (is_punctuator('*') ||
         is_punctuator('/') ||
         is_punctuator('%')) {
    auto be = std::make_unique<BinExpr>();
    be->op = tok->i;
    be->lhs = e.release();

    next_token();
    be->rhs = primary_expression();
    if (!be->rhs)
      error("expecting expression after %c", be->op);

    e = std::move(be);
  }

  return e.release();
}

// [expr.prim]
Expr* Parser::primary_expression()
{
  if (is_punctuator('(')) {
    next_token();               // Skip '('
    std::unique_ptr<Expr> e(expression());
    if (!is_punctuator(')'))
      error("expected ')' to finish expression");
    next_token();
    return e.release();
  }
  else if (is_punctuator('*') ||
           is_punctuator('&') ||
           is_punctuator('+') ||
           is_punctuator('-') ||
           is_punctuator('!') ||
           is_punctuator('~')) {
    auto be = std::make_unique<UnaryExpr>();
    be->op = tok->i;
    next_token();
    be->operand = primary_expression();
    if (!be->operand)
      error("expected primary expression after '-'");
    return be.release();
  }
  else if (is(TokenKind::NumericConstant)) {
    auto l = std::make_unique<Literal>();
    l->value = std::strtol(lex_data->id_text(*tok).c_str(), nullptr, 0);
    next_token();
    return l.release();
  }
  return nullptr;
}
