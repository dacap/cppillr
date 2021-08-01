// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

struct Options {
  std::string command;
  std::string print;
  std::vector<std::string> parse_files;
  int threads;
  bool show_time = false;
  bool show_tokens = false;
  bool show_ast = false;
  bool show_includes = false;
  bool show_functions = false;
  bool count_tokens = false;
  bool count_lines = false;
  bool keyword_stats = false;
};
