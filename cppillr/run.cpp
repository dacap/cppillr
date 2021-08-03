// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "cppillr/run.h"

#include "cppillr/program.h"

#include <vector>

namespace run {

struct VM {
  std::vector<int> stack;
};

static void run_node(
  Node* n,
  Program& p,
  VM& vm)
{
  switch (n->kind) {

    case NodeKind::UnaryExpr: {
      auto ue = static_cast<UnaryExpr*>(n);
      run_node(ue->operand, p, vm);
      if (!vm.stack.empty()) {
        switch (ue->op) {
          case '*': break; // TODO
          case '&': break; // TODO
          case '+': break;
          case '-': vm.stack.back() = -vm.stack.back(); break;
          case '!': vm.stack.back() = !vm.stack.back(); break;
          case '~': vm.stack.back() = ~vm.stack.back(); break;
        }
      }
      break;
    }

    case NodeKind::BinExpr: {
      auto be = static_cast<BinExpr*>(n);
      run_node(be->lhs, p, vm);
      run_node(be->rhs, p, vm);

      if (vm.stack.size() >= 2) {
        int x = vm.stack[vm.stack.size()-2];
        int y = vm.stack[vm.stack.size()-1];

        switch (be->op) {
          case '+': x += y; break;
          case '-': x -= y; break;
          case '*': x *= y; break;
          case '/': x /= y; break;
          case '%': x %= y; break;
        }

        vm.stack.pop_back();
        vm.stack.back() = x;
      }
      break;
    }

    case NodeKind::Literal: {
      auto l = static_cast<Literal*>(n);
      vm.stack.push_back(l->value);
      break;
    }

    case NodeKind::Return: {
      auto r = static_cast<Return*>(n);
      // Run the return expression (the result should be left in the stack)
      if (r->expr)
        run_node(r->expr, p, vm);
      break;
    }

    case NodeKind::CompoundStmt: {
      auto e = static_cast<CompoundStmt*>(n);
      for (Stmt* stmt : e->stmts) {
        run_node(stmt, p, vm);
      }
      break;
    }

    case NodeKind::Function: {
      auto f = static_cast<FunctionNode*>(n);
      const LexData& lex_data = p.lex_data[f->body->lex_i];

      // Parse function that was just fast-parsed (only tokens)
      if (!f->body->block) {
        Parser parser;
        parser.parse_function_body(lex_data, f);

        if (!f->body->block) {
          std::printf("error parsing %s() function body", f->name.c_str());
          std::exit(1);
        }
      }

      run_node(f->body->block, p, vm);
      break;
    }

  }
}

int run(
  const Options& options,
  thread_pool& pool,
  Program& prog)
{
  std::vector<FunctionNode*> candidates;
  int ret_value = 1;

  // Search 'main' function and start executing statement nodes from
  // there.
  for (const auto& data : prog.parser_data) {
    for (FunctionNode* f : data.functions) {
      if (f->name == "main") {
        candidates.push_back(f);
      }
    }
  }

  if (candidates.empty()) {
    std::printf("no main() function found");
  }
  else if (candidates.size() != 1) {
    std::printf("multiple main() functions found:");
    // TODO print location of all main() functions
  }
  else {
    VM vm;
    run_node(candidates.front(), prog, vm);
    if (!vm.stack.empty())
      ret_value = vm.stack[0];
    else
      ret_value = 0;
  }
  return ret_value;
}

} // namespace run
