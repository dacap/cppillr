// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

class thread_pool;
struct Options;
struct Program;

namespace run {

int run(
  const Options& options,
  thread_pool& pool,
  Program& prog);

} // namespace run
