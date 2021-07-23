// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

namespace run {

void run_function(
  const std::vector<LexData>& lex_data,
  const std::vector<ParserData>& parser_data,
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
  const std::vector<LexData>& lex_data,
  const std::vector<ParserData>& parser_data)
{
  std::vector<FunctionNode*> candidates;

  // Search 'main' function and start executing statement nodes from
  // there.
  for (const auto& data : parser_data) {
    for (Node* n : data.functions) {
      if (n->kind == NodeKind::Function) {
        auto f = static_cast<FunctionNode*>(n);
        if (f->name == "main") {
          candidates.push_back(f);
        }
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
    run_function(lex_data,
                 parser_data,
                 candidates.front());
  }
}

} // namespace run
