// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "cppillr/docs.h"
#include "cppillr/keywords.h"
#include "cppillr/options.h"
#include "cppillr/program.h"
#include "cppillr/run.h"
#include "utils/stopwatch.h"
#include "utils/thread_pool.h"

#include <cstring>
#include <fstream>

//////////////////////////////////////////////////////////////////////
// tools

void show_tokens(const LexData& data)
{
  std::printf("%s: tokens=%d\n",
              data.fn.c_str(),
              int(data.tokens.size()));

  int i = 0;
  for (auto& tok : data.tokens) {
    std::printf("%s:%d:%d: [%d] ",
                data.fn.c_str(),
                tok.pos.line, tok.pos.col, i);
    switch (tok.kind) {
      case TokenKind::PPBegin:
        std::printf("PP { \n");
        break;
      case TokenKind::PPKeyword:
        std::printf("PPKEY %s\n", pp_keywords_id[tok.i].c_str());
        break;
      case TokenKind::PPHeaderName:
        std::printf("PP.H ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::PPEnd:
        std::printf("} PP\n");
        break;
      case TokenKind::Comment:
        std::printf("COMMENT ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.comments[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Identifier:
        std::printf("ID ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Literal:
        std::printf("LIT ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::CharConstant:
        std::printf("CHR ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::NumericConstant:
        std::printf("NUM ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Keyword:
        std::printf("KEY %s\n", keywords_id[tok.i].c_str());
        break;
      case TokenKind::Punctuator:
        if (tok.j)
          std::printf("OP %c%c\n", (char)tok.i, (char)tok.j);
        else
          std::printf("OP %c\n", (char)tok.i);
        break;
    }
    ++i;
  }
}

struct PPIf {
  std::string id;
  bool def;
};

void show_includes(const LexData& data)
{
  std::vector<PPIf> stack;

  std::printf("%s: includes\n",
              data.fn.c_str());

  for (int i=0; i+2<int(data.tokens.size()); ++i) {
    if (data.tokens[i].kind == TokenKind::PPBegin &&
        data.tokens[i+1].kind == TokenKind::PPKeyword) {
      if (data.tokens[i+1].i == pp_key_if ||
          data.tokens[i+1].i == pp_key_ifdef ||
          data.tokens[i+1].i == pp_key_ifndef) {
        PPIf ppif;
        switch (data.tokens[i+1].i) {
          case pp_key_if: {
            bool first = false;
            ppif.id = "#if (";
            for (i+=2; i<int(data.tokens.size()) &&
                   data.tokens[i].kind != TokenKind::PPEnd; ++i) {
              if (data.tokens[i].kind == TokenKind::Identifier ||
                  data.tokens[i].kind == TokenKind::Literal) {
                std::string lit(data.ids.begin()+data.tokens[i].i,
                                data.ids.begin()+data.tokens[i].j);
                if (first)
                  first = false;
                else
                  ppif.id.push_back(' ');
                ppif.id += lit;
              }
            }
            ppif.id += ")";
            ppif.def = true;
            break;
          }
          case pp_key_ifndef:
          case pp_key_ifdef: {
            std::string id(data.ids.begin()+data.tokens[i+2].i,
                           data.ids.begin()+data.tokens[i+2].j);
            ppif.id = id;
            ppif.def = (data.tokens[i+1].i == pp_key_ifdef);
            break;
          }
        }
        stack.push_back(ppif);
      }
      else if (data.tokens[i+1].i == pp_key_endif) {
        if (!stack.empty())
          stack.erase(--stack.end());
      }
      else if (data.tokens[i+1].i == pp_key_include &&
               data.tokens[i+2].kind == TokenKind::PPHeaderName) {
        std::string str(data.ids.begin()+data.tokens[i+2].i,
                        data.ids.begin()+data.tokens[i+2].j);
        std::printf("  %s", str.c_str());
        if (!stack.empty()) {
          std::printf(" (");
          for (int i=0; i<int(stack.size()); ++i) {
            if (i > 0)
              std::printf(" && ");
            std::printf("%s%s",
                        stack[i].def ? "": "!",
                        stack[i].id.c_str());
          }
          std::printf(")");
        }
        std::printf("\n");
      }
    }
  }
}

void show_ast_node(Node* n, int indent)
{
  for (int i=0; i<indent; ++i)
    std::printf("  ");

  switch (n->kind) {

    case NodeKind::BinExpr: {
      auto be = static_cast<BinExpr*>(n);
      std::printf("BinExpr %c\n", be->op);
      show_ast_node(be->lhs, indent+1);
      show_ast_node(be->rhs, indent+1);
      break;
    }

    case NodeKind::Literal: {
      auto l = static_cast<Literal*>(n);
      std::printf("Literal %d\n", l->value);
      break;
    }

    case NodeKind::Return: {
      auto r = static_cast<Return*>(n);
      std::printf("Return\n");
      if (r->expr)
        show_ast_node(r->expr, indent+1);
      break;
    }

    case NodeKind::CompoundStmt: {
      auto e = static_cast<CompoundStmt*>(n);
      std::printf("CompoundStmt\n");
      for (Stmt* stmt : e->stmts) {
        show_ast_node(stmt, indent+1);
      }
      break;
    }

    case NodeKind::Function: {
      auto f = static_cast<FunctionNode*>(n);
      std::printf("Function %s\n", f->name.c_str());
      show_ast_node(f->body->block, indent+1);
      break;
    }

  }
}

void show_ast(const ParserData& data)
{
  for (FunctionNode* f : data.functions) {
    show_ast_node(f, 0);
  }
}

void show_functions(const ParserData& data,
                    const bool show_tokens)
{
  for (FunctionNode* f : data.functions) {
    std::printf("function %s()",
                f->name.c_str());
    if (show_tokens)
      std::printf(" body tokens [%d,%d]",
                  f->body->beg_tok,
                  f->body->end_tok);
    std::printf("\n");

    for (ParamNode* p : f->params->params) {
      std::printf(" param %s %s\n",
                  keywords_id[p->builtin_type].c_str(),
                  p->name.c_str());
    }
  }
}


int count_lines(const LexData& data)
{
  int nlines = 0;
  int line = 0;
  for (auto& tok : data.tokens) {
    if (line != tok.pos.line) {
      line = tok.pos.line;
      ++nlines;
    }
  }
  return nlines;
}

//////////////////////////////////////////////////////////////////////
// KeywordStats

class KeywordStats {
  std::array<size_t, MaxKeyword> keywords;
public:
  KeywordStats() { keywords.fill(0); }

  void add(const LexData& data) {
    for (auto& tok : data.tokens) {
      if (tok.kind == TokenKind::Keyword)
        ++keywords[tok.i];
    }
  }

  void print() {
    for (int i=0; i<int(MaxKeyword); ++i) {
      if (keywords[i] > 0)
        std::printf("%d\t%s\n", keywords[i], keywords_id[i].c_str());
    }
  }
};

//////////////////////////////////////////////////////////////////////
// main

bool parse_options(int argc, char* argv[], Options& options)
{
  for (int i=1; i<argc; ++i) {
    // Ignore empty args?
    if (argv[i][0] != '-') {
      if (options.command.empty())
        options.command = argv[i];
      else
        options.parse_files.push_back(argv[i]);
      continue;
    }
    else if (options.command.empty()) {
      options.command = "none";
    }

    if (std::strcmp(argv[i], "-h") == 0) {
      std::printf("%s [-h] [directory | files.cpp]",
                  argv[0]);
      return false;
    }
    else if (std::strcmp(argv[i], "-filelist") == 0) {
      ++i;
      if (i < argc) {
        std::ifstream f(argv[i]);
        std::string line;
        while (std::getline(f, line)) {
          options.parse_files.push_back(line);
        }
      }
    }
    else if (std::strcmp(argv[i], "-print") == 0) {
      ++i;
      if (i < argc) {
        options.print = argv[i];
      }
    }
    else if (std::strcmp(argv[i], "-showtime") == 0) {
      options.show_time = true;
    }
    else if (std::strcmp(argv[i], "-showtokens") == 0) {
      options.show_tokens = true;
    }
    else if (std::strcmp(argv[i], "-showast") == 0) {
      options.show_ast = true;
    }
    else if (std::strcmp(argv[i], "-showincludes") == 0) {
      options.show_includes = true;
    }
    else if (std::strcmp(argv[i], "-showfunctions") == 0) {
      options.show_functions = true;
    }
    else if (std::strcmp(argv[i], "-counttokens") == 0) {
      options.count_tokens = true;
    }
    else if (std::strcmp(argv[i], "-countlines") == 0) {
      options.count_lines = true;
    }
    else if (std::strcmp(argv[i], "-keywordstats") == 0) {
      options.keyword_stats = true;
    }
    else if (std::strcmp(argv[i], "-threads") == 0) {
      ++i;
      if (i < argc) {
        options.threads = std::strtol(argv[i], nullptr, 10);
      }
    }
    else if (std::strcmp(argv[i], "--") == 0) {
      ++i;
      options.parse_files.push_back(std::string()); // parse stdin
    }
    else {
      std::printf("%s: invalid argument %s\n", argv[0], argv[i]);
      return false;
    }
  }
  return true;
}

int run_with_options(const Options& options)
{
  std::printf("running command \"%s\"\n", options.command.c_str());
  int ret_value = 0;

  Stopwatch t;
  thread_pool pool(options.threads);
  Program prog;

  for (const auto& fn : options.parse_files) {
    pool.execute(
      [&pool, fn, &prog]{
        Lexer lexer;
        lexer.lex(fn);

        int i = prog.add_lex(lexer.move_data());

        pool.execute(
          [i, &prog]{
            LexData data;
            prog.get_lex(i, data);

            Parser parser(i);
            parser.parse(data);

            prog.add_parser_data(parser.move_data());
          });
      });
  }
  pool.wait_all();

  if (options.show_time)
    t.watch("parse files");

  if (options.command == "docs")
    docs::run(options, pool, prog);
  else if (options.command == "run")
    ret_value = run::run(options, pool, prog);

  if (options.count_tokens) {
    int total_tokens = 0;
    for (auto& data : prog.lex_data)
      total_tokens += data.tokens.size();

    std::printf("total tokens %d\n", total_tokens);
  }

  if (options.count_lines) {
    int total_lines = 0;
    for (auto& data : prog.lex_data)
      total_lines += count_lines(data);

    std::printf("total lines %d\n", total_lines);
  }

  if (options.keyword_stats) {
    KeywordStats keyword_stats;
    for (auto& data : prog.lex_data)
      keyword_stats.add(data);

    keyword_stats.print();
  }

  if (options.show_tokens) {
    for (auto& data : prog.lex_data)
      show_tokens(data);
  }

  if (options.show_ast) {
    for (auto& data : prog.parser_data)
      show_ast(data);
  }

  if (options.show_includes) {
    for (auto& data : prog.lex_data)
      show_includes(data);
  }

  if (options.show_functions) {
    for (const auto& data : prog.parser_data)
      show_functions(data, options.show_tokens);
  }

  return ret_value;
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::printf("%s: no input file\n", argv[0]);
    return 1;
  }

  Options options;
  options.threads = std::thread::hardware_concurrency();
  if (!parse_options(argc, argv, options))
    return 0;

  create_keyword_tables();
  return run_with_options(options);
}
