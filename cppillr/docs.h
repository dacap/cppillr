// Copyright (C) 2020-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <cstdarg>

namespace docs {

struct DocSection {
  int level = 1;
  std::string id, type, line, desc;
};

struct Doc {
  std::vector<DocSection> sections;
};

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

class Parser {
  const LexData& data;
  Token tok;
  std::vector<Token>::const_iterator tok_it, tok_end;

  // Reads next token
  Token& next_token() {
    if (tok_it != tok_end) {
      tok = *tok_it;
      ++tok_it;
    }
    else {
      tok = Token(TokenKind::Eof, TextPos{0, 0});
    }
    return tok;
  }

  bool is(TokenKind kind) const {
    return tok.kind == kind;
  }

  bool eof() const {
    return is(TokenKind::Eof);
  }

  template<typename ...Args>
  void error(Args&& ...args) {
    char buf[4096];
    std::sprintf(buf, std::forward<Args>(args)...);
    std::printf("%s:%d:%d: %s\n",
                data.fn.c_str(),
                tok.pos.line,
                tok.pos.col,
                buf);
    std::exit(1);
  }

public:
  Parser(const LexData& data)
    : data(data)
    , tok(TokenKind::Eof, TextPos{0, 0})
    , tok_it(data.tokens.begin())
    , tok_end(data.tokens.end()) { }

  void createDoc(Doc& doc) {
    while (next_token().kind != TokenKind::Eof) {
      // Discard tokens until we find a comment, which might be
      // included in the docs.
      if (!is(TokenKind::Comment))
        continue;

      Token commentTok = tok;
      tok = next_token();
      if (eof()) // end of tokens (a comment at the end of file, maybe commenting the file?)
        break;

      switch (tok.kind) {
        case TokenKind::Keyword:
          switch (tok.i) {
            // Structures and namespaces
            case key_class:
            case key_struct:
            case key_enum:
            case key_union:
            case key_namespace: {
              Token idTok = next_token();
              if (!is(TokenKind::Identifier)) {
                error("expecting identifier after %s",
                      keywords_id[int(tok.i)].c_str());
                break;
              }

              doc.sections.push_back(
                make_section(data, commentTok,
                             data.id_text(idTok),
                             keywords_id[int(tok.i)]));
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
              Token id_tok = next_token();
              if (!is(TokenKind::Identifier))
                error("expecting identifier");

              doc.sections.push_back(
                make_section(data, commentTok,
                             data.id_text(id_tok),
                             keywords_id[int(tok.i)]));
              break;
            }
          }
          break;
          // A user defined type to define a return value of a function or a variable type
        case TokenKind::Identifier: {
          Token firstTok = tok;
          std::string type;

          // TODO types that start with "const" go to the keyword
          //      case, or with "::" go to punctuator case

          tok = next_token();
          if (tok.is_double_colon()) {
            type += "::";
            tok = next_token();
          }

          if (!is(TokenKind::Identifier))
            error("expecting identifier");

          type += data.id_text(tok);
          next_token();

          while (tok.is_double_colon()) {
            type += "::";
            next_token();
            if (!is(TokenKind::Identifier))
              error("expecting identifier after ::");

            type += data.id_text(tok);
            next_token();
          }

          while (!eof() &&
                 // Pointers and references
                 ((is(TokenKind::Punctuator) &&
                   ((tok.i == '*' && tok.j == 0) ||
                    (tok.i == '&' && tok.j == 0)))
                  ||
                  // const
                  (tok.is_const_keyword()))) {
            if (is(TokenKind::Keyword)) {
              type.push_back(' ');
              type += keywords_id[tok.i];
            }
            else
              type.push_back(tok.i); // Add '*' or '&'
            next_token();
          }

          if (!is(TokenKind::Identifier))
            error("expecting identifier after type %s", type.c_str());

          std::string id(data.id_text(tok));

          doc.sections.push_back(
            make_section(data, commentTok, id, type));
          break;
        }
      }
    }
  }
};

Doc process_file(const LexData& data)
{
  Doc doc;
  Parser parser(data);
  parser.createDoc(doc);
  return std::move(doc);
}

void run(
  const Options& options,
  thread_pool& pool,
  const Program& prog)
{
  std::mutex docs_mutex;
  std::vector<Doc> docs;

  for (const auto& data : prog.lex_data) {
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
      std::string templ = options.print;
      replace_string(templ, "{id}", sec.id);
      replace_string(templ, "{type}", sec.type);
      replace_string(templ, "{line}", sec.line);
      replace_string(templ, "{desc}", sec.desc);
      if (!templ.empty())
        std::puts(templ.c_str());
    }
  }
}

} // namespace docs
