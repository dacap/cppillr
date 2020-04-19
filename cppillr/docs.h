// Copyright (C) 2020  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

namespace docs {

struct DocSection {
  int level = 1;
  std::string title;
  std::string desc;
};

struct Doc {
  std::vector<DocSection> sections;
};

enum class DocCtx {
  Namespace,
  Function,
  Enum,
};

void error(const char* fmt, ...)
{
  va_list vlist;
  va_start(vlist, fmt);
  std::vfprintf(stderr, fmt, vlist);
  va_end(vlist);
}

void run(
  const Options& options,
  thread_pool& pool,
  const std::vector<LexData>& lex_data,
  const std::vector<ParserData>& parser_data)
{
  std::mutex docs_mutex;
  std::vector<Doc> docs;

  for (const auto& data : lex_data) {
    pool.execute(
      [&data, &docs_mutex, &docs]() {
        std::vector<DocCtx> ctx;
        Doc doc;
        int n = int(data.tokens.size());

        for (int i=0; i<n; ++i) {
          // A comment, ok, it might be included in the docs
          if (data.tokens[i].kind == TokenKind::Comment) {
            if (i+1 == n) // end of tokens (a comment at the end of file, maybe commenting the file?)
              break;

            auto& tok = data.tokens[i];

            switch (data.tokens[i+1].kind) {
              case TokenKind::Keyword:
                switch (data.tokens[i+1].i) {
                  // Structures and namespaces
                  case key_class:
                  case key_struct:
                  case key_enum:
                  case key_union:
                  case key_namespace: {
                    if (i+2 == n ||
                        data.tokens[i+2].kind != TokenKind::Identifier) {
                      error("expecting identifier after %s",
                            keywords_id[int(data.tokens[i+1].i)].c_str());
                      break;
                    }

                    std::string id(data.ids.begin()+data.tokens[i+2].i,
                                   data.ids.begin()+data.tokens[i+2].j);

                    DocSection sec;
                    sec.title = id + " (" + keywords_id[int(data.tokens[i+1].i)] + ")";

                    char buf[1024*2];
                    std::sprintf(buf, "%s:%d:%d:\n\n",
                                 data.fn.c_str(),
                                 tok.pos.line, tok.pos.col);
                    sec.desc = buf;
                    sec.desc += std::string(data.comments.begin()+data.tokens[i].i,
                                           data.comments.begin()+data.tokens[i].j);
                    doc.sections.push_back(sec);
                    break;
                  }
                  // Variables or functions
                  case key_auto:
                  case key_bool:
                  case key_char:
                  case key_char16_t:
                  case key_char32_t:
                  case key_char8_t:
                  case key_const:
                  case key_constexpr:
                  case key_constinit:
                  case key_double:
                  case key_explicit:
                  case key_export:
                  case key_extern:
                  case key_float:
                  case key_inline:
                  case key_int:
                  case key_long:
                  case key_mutable:
                  case key_register:
                  case key_short:
                  case key_signed:
                  case key_static:
                  case key_template:
                  case key_thread_local:
                  case key_typedef:
                  case key_unsigned:
                  case key_using:
                  case key_virtual:
                  case key_void:
                  case key_volatile:
                  case key_wchar_t: {
                    if (i+2 == n ||
                        data.tokens[i+2].kind != TokenKind::Identifier)
                      error("expecting identifier");

                    std::string id(data.ids.begin()+data.tokens[i+2].i,
                                   data.ids.begin()+data.tokens[i+2].j);

                    DocSection sec;
                    sec.title = id + " (" + keywords_id[int(data.tokens[i+1].i)] + ")";

                    char buf[1024*2];
                    std::sprintf(buf, "%s:%d:%d:\n\n",
                                 data.fn.c_str(),
                                 tok.pos.line, tok.pos.col);
                    sec.desc = buf;
                    sec.desc += std::string(data.comments.begin()+data.tokens[i].i,
                                           data.comments.begin()+data.tokens[i].j);

                    std::string(sec.desc);
                    doc.sections.push_back(sec);
                    break;
                  }
                }
                break;
              // A user defined type to define a return value of a function or a variable type
              case TokenKind::Identifier: {
                // TODO
                break;
              }
            }
          }
        }

        {
          std::unique_lock<std::mutex> l(docs_mutex);
          docs.emplace_back(std::move(doc));
        }
      });
  }

  pool.wait_all();

  // Generate markdown file

  for (const Doc& doc : docs) {
    for (const DocSection& sec : doc.sections) {
      for (int i=0; i<sec.level; ++i)
        std::printf("#");
      std::printf(" %s\n\n%s\n", sec.title.c_str(), sec.desc.c_str());
    }
  }
}

} // namespace docs
