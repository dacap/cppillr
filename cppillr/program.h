// Copyright (C) 2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include "cppillr/lexer.h"
#include "cppillr/parser.h"

#include <mutex>
#include <vector>

// Data collected from the source code (tokens + AST nodes)
class Program {
  mutable std::mutex lex_mutex;
  mutable std::mutex parser_mutex;
public:
  std::vector<LexData> lex_data;
  std::vector<ParserData> parser_data;

  int add_lex(LexData&& lex) {
    std::unique_lock<std::mutex> l(lex_mutex);
    int i = int(lex_data.size());
    lex_data.emplace_back(std::move(lex));
    return i;
  }

  void get_lex(int i, LexData& output) const {
    std::unique_lock<std::mutex> l(lex_mutex);
    output = lex_data[i];
  }

  void add_parser_data(ParserData&& data) {
    std::unique_lock<std::mutex> l(parser_mutex);
    parser_data.emplace_back(std::move(data));
  }

};
