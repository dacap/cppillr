// Copyright (C) 2020  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

namespace run {

void run(
  const Options& options,
  thread_pool& pool,
  const std::vector<LexData>& lex_data,
  const std::vector<ParserData>& parser_data)
{
  enum State { Outside, PP, Include };

  for (const auto& data : lex_data) {
    pool.execute(
      [&data]() {
        State state = State::Outside;
        for (auto& tok : data.tokens) {
          if (tok.kind == TokenKind::PPBegin) {
            if (state == State::Outside)
              state = State::PP;
          }
          else if (tok.kind == TokenKind::PPEnd) {
            if (state == State::PP)
              state = State::Outside;
          }
          else if (state == State::PP) {
            if (tok.kind == TokenKind::PPKeyword &&
                tok.i == pp_key_include) {
              state = State::Include;
            }
          }
          else if (state == State::Include) {
            // TODO match header file name with library for the linker
            std::string headerFile;
            for (int i=tok.i; i<tok.j; ++i)
              headerFile.push_back(data.ids[i]);
            std::printf("%s\n", headerFile.c_str());
            state = State::PP;
          }
        }
      });
  }

  pool.wait_all();
}

} // namespace run
