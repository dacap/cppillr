// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include <string>
#include <vector>

class thread_pool;
struct Options;
struct Program;

namespace docs {

struct DocSection {
  int level = 1;
  std::string id, type, line, desc;
};

struct Doc {
  std::vector<DocSection> sections;
};

void run(
  const Options& options,
  thread_pool& pool,
  const Program& prog);

} // namespace docs
