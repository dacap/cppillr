// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

namespace run {

static void run_function(
  const Program& p,
  FunctionNode* f)
{
  // TODO
  std::printf("calling main(), body tokens [%d,%d]\n",
              f->body->beg_tok,
              f->body->end_tok);
}

void run(
  const Options& options,
  thread_pool& pool,
  const Program& prog)
{
  std::vector<FunctionNode*> candidates;

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
    run_function(prog, candidates.front());
  }
}

} // namespace run
