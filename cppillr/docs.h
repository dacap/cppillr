// Copyright (C) 2020  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

namespace docs {

struct DocSection {
  int level = 1;
  std::string id, type, line, desc;
};

struct Doc {
  std::vector<DocSection> sections;
};

void error(const char* fmt, ...)
{
  va_list vlist;
  va_start(vlist, fmt);
  std::vfprintf(stderr, fmt, vlist);
  va_end(vlist);
}

DocSection make_section(const LexData& data,
                        const Token& comment_tok,
                        const std::string& id,
                        const std::string& type)
{
  DocSection sec;
  sec.id = id;
  sec.type = type;
  trim_string(sec.id);
  trim_string(sec.type);

  char buf[1024*2];
  std::sprintf(buf, "%s:%d:%d",
               data.fn.c_str(),
               comment_tok.pos.line,
               comment_tok.pos.col);
  sec.line = buf;
  sec.desc = data.comment_text(comment_tok);
  trim_string(sec.desc);
  return sec;
}

Doc process_file(const LexData& data)
{
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
                error("expecting identifier after %s\n",
                      keywords_id[int(data.tokens[i+1].i)].c_str());
                break;
              }

              doc.sections.push_back(
                make_section(data, tok,
                             data.id_text(data.tokens[i+2]),
                             keywords_id[int(data.tokens[i+1].i)]));
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
                error("expecting identifier\n");

              doc.sections.push_back(
                make_section(data, tok,
                             data.id_text(data.tokens[i+2]),
                             keywords_id[int(data.tokens[i+1].i)]));
              break;
            }
          }
          break;
          // A user defined type to define a return value of a function or a variable type
        case TokenKind::Identifier: {
          ++i;

          std::string type;

          // TODO types that start with "const" go to the keyword
          //      case, or with "::" go to punctuator case

          if (data.tokens[i].is_double_colon()) {
            type += "::";
            ++i;
          }

          if (i >= n ||
              data.tokens[i].kind != TokenKind::Identifier)
            error("expecting identifier\n");

          type += data.id_text(data.tokens[i]);
          ++i;

          if (i >= n)
            error("expecting type and identifier\n");

          while (data.tokens[i].is_double_colon()) {
            type += "::";
            ++i;
            if (i == n ||
                data.tokens[i].kind != TokenKind::Identifier)
              error("expecting identifier after ::\n");

            type += data.id_text(data.tokens[i]);
            ++i;
          }

          while (i < n &&
                 // Pointers and references
                 ((data.tokens[i].kind == TokenKind::Punctuator &&
                   ((data.tokens[i].i == '*' && data.tokens[i].j == 0) ||
                    (data.tokens[i].i == '&' && data.tokens[i].j == 0)))
                  ||
                  // const
                  (data.tokens[i].is_const_keyword()))) {
            if (data.tokens[i].kind == TokenKind::Keyword) {
              type.push_back(' ');
              type += keywords_id[data.tokens[i].i];
            }
            else
              type.push_back(data.tokens[i].i);
            ++i;
          }

          if (i == n ||
              data.tokens[i].kind != TokenKind::Identifier)
            error("expecting identifier after type %s", type.c_str());

          std::string id(data.id_text(data.tokens[i]));

          doc.sections.push_back(
            make_section(data, tok, id, type));
          break;
        }
      }
    }
  }
  return std::move(doc);
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
        Doc doc = process_file(data);
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
#if 1
      std::printf("%s: %s (%s)\n", sec.line.c_str(), sec.id.c_str(), sec.type.c_str());
#else
      for (int i=0; i<sec.level; ++i)
        std::printf("#");
      std::printf(" %s\n\n%s\n", sec.title.c_str(), sec.desc.c_str());
#endif
    }
  }
}

} // namespace docs
