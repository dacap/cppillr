// Copyright (C) 2019-2020  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "utils/scoped_fclose.h"
#include "utils/stopwatch.h"
#include "utils/thread_pool.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <map>
#include <vector>

void trim_string(std::string& s)
{
  int i;

  if (!s.empty()) {
    for (i=int(s.size())-1; i>=0 && isspace(s[i]); --i)
      ;
    s.erase(s.begin()+i+1, s.end());
  }

  if (!s.empty()) {
    for (i=0; i<int(s.size()) && isspace(s[i]); ++i)
      ;
    s.erase(s.begin(), s.begin()+i);
  }
}

void replace_string(
  std::string& subject,
  const std::string& replace_this,
  const std::string& with_that)
{
  if (replace_this.empty())
    return;

  std::size_t i = 0;
  while (true) {
    i = subject.find(replace_this, i);
    if (i == std::string::npos)
      break;
    subject.replace(i, replace_this.size(), with_that);
    i += with_that.size();
  }
}

//////////////////////////////////////////////////////////////////////
// lexer

enum PPKeyword {
  #define PP_KEYWORD(x) pp_key_##x,
  #include "keywords.h"
  MaxPPKeyword
};
enum Keyword {
  #define KEYWORD(x) key_##x,
  #include "keywords.h"
  MaxKeyword
};
std::unordered_map<std::string, PPKeyword> pp_keywords;
std::unordered_map<std::string, Keyword> keywords;
std::vector<std::string> pp_keywords_id, keywords_id;

enum class TokenKind {
  PPBegin,
  PPKeyword,
  PPHeaderName,
  PPEnd,
  Comment,
  Identifier,
  Keyword,
  CharConstant,
  Literal,
  NumericConstant,
  Punctuator,
  Eof,
};

struct TextPos {
  int line, col;
  TextPos(int line = 0, int col = 0) : line(line), col(col) { }
};

struct Token {
  TokenKind kind;
  TextPos pos;
  // Depending on the kind of token these two variables have different meanings:
  // * TokenKind::Identifier: i and j are the start and end of a
  //   string from LexData.ids
  // * TokenKind::PPKeyword: i is a "enum PPKeyword" value
  // * TokenKind::Keyword: i is a "enum Keyword" value
  // * TokenKind::Punctuator: i is the first operand char (e.g. '<') and j the second one (e.g. '=')
  int i, j;

  Token(TokenKind kind, const TextPos& pos, int i = 0, int j = 0)
    : kind(kind), pos(pos), i(i), j(j) { }

  bool is_const_keyword() const {
    return (kind == TokenKind::Keyword && i == key_const);
  }

  bool is_double_colon() const {
    return (kind == TokenKind::Punctuator && i == ':' && j == ':');
  }
};

// The lexer works like a finite-state machine where it's mainly
// reading whitespace (ReadingWhitespace) i.e. discarding data from
// file, and when something interesting is found it changes to a state
// to read the specific token, and then returns back to its normal
// state (ReadingWhitespace).
enum class LexState {
  ReadingWhitespace,
  ReadingWhitespaceToEOL,
  ReadingIdentifier,
  ReadingLineComment,
  ReadingMultilineComment,
  ReadingBeforeHeaderName,
  ReadingSysHeaderName,
  ReadingUserHeaderName,
  ReadingErrorTextToEOL,
  ReadingString,
  ReadingWideString,
  ReadingChar,
  ReadingWideChar,
  ReadingHexadecimal,
  ReadingBinary,
  ReadingOctal,
  ReadingIntegerPart,
  ReadingDecimalPart,
};

struct LexData {
  std::string fn;
  std::vector<uint8_t> ids;
  std::vector<uint8_t> comments;
  std::vector<Token> tokens;
  int readed_bytes;

  template<typename ...Args>
  void add_token(Args&& ...args) {
    tokens.emplace_back<Args...>(std::forward<Args>(args)...);
  }

  std::string id_text(const Token& tok) const {
    return std::string(ids.begin()+tok.i,
                       ids.begin()+tok.j);
  }

  std::string comment_text(const Token& tok) const {
    return std::string(comments.begin()+tok.i,
                       comments.begin()+tok.j);
  }
};

class CharReader {
  std::FILE* f;
  std::array<uint8_t, 1024> buf;
  std::array<uint8_t, 1024>::iterator end = buf.end();
  std::array<uint8_t, 1024>::iterator it = end;
  int readed_bytes_ = 0;
  TextPos pos_ = { 1, 0 };
public:
  CharReader() : f(nullptr) { }

  void set_file(std::FILE* f) { this->f = f; }
  bool eof() const { return std::feof(f); }

  int readed_bytes() const { return readed_bytes_; }
  const TextPos& pos() const { return pos_; }

  int nextchar() {
    if (it == end) {
      if (eof())
        return 0;
      int bytes = std::fread(&buf[0], 1, buf.size(), f);
      if (bytes == 0)
        return 0;
      readed_bytes_ += bytes;
      it = buf.begin();
      end = buf.begin() + bytes;
    }
    int chr = *it;
    if (chr == '\n') {
      ++pos_.line;
      pos_.col = 0;
    }
    else
      ++pos_.col;
    ++it;
    return chr;
  }

};

class Lexer {
public:
  enum class Result { ErrorOpeningFile, OK };

  Result lex(const std::string& fn);
  LexData&& move_data() { return std::move(data); }

private:
  enum class Action {
    // Read next char from input file and process it
    NextChr,
    // Don't read the next char, process the current "Lexer::chr"
    ProcessChr,
  };

  Action process();

  template<typename ...Args>
  void add_token(Args&& ...args) {
    data.tokens.emplace_back<Args...>(std::forward<Args>(args)...);
  }

  void add_token_id(TokenKind tokenKind) {
    if (!tok_id.empty())
      data.ids.insert(data.ids.end(),
                      (uint8_t*)&tok_id[0],
                      (uint8_t*)&tok_id[0]+tok_id.size());
    data.add_token(tokenKind,
                   reader.pos(),
                   data.ids.size()-tok_id.size(),
                   data.ids.size());
    tok_id.clear();
  }

  void add_token_comment() {
    trim_string(tok_id);
    if (!tok_id.empty()) {
      data.comments.insert(data.comments.end(),
                           (uint8_t*)&tok_id[0],
                           (uint8_t*)&tok_id[0]+tok_id.size());

      // Merge comments
      if (!data.tokens.empty() &&
          data.tokens.back().kind == TokenKind::Comment) {
        data.tokens.back().j = data.comments.size();
      }
      else {
        data.add_token(TokenKind::Comment,
                       reader.pos(),
                       data.comments.size()-tok_id.size(),
                       data.comments.size());
      }

      tok_id.clear();
    }
  }

  template<typename ...Args>
  void error(Args&& ...args) {
    char buf[4096];
    std::sprintf(buf, std::forward<Args>(args)...);
    std::printf("%s:%d:%d: %s\n",
                data.fn.c_str(),
                reader.pos().line,
                reader.pos().col,
                buf);
    std::exit(1);
  }

  LexState state;
  LexData data;
  CharReader reader;
  int chr;     // Current char from input being processed
  bool prepro; // True if we are reading preprocessor tokens.
  std::string tok_id;
  bool keep_comments = true;
};

Lexer::Result Lexer::lex(const std::string& fn)
{
  std::FILE* f = std::fopen(fn.c_str(), "r");
  if (!f)
    return Lexer::Result::ErrorOpeningFile;

  Scoped_fclose fc(f);

  data.fn = fn;
  data.tokens.clear();
  data.tokens.reserve(128); // TODO This number might depend on the
                            //      size of the input file
  state = LexState::ReadingWhitespace;
  prepro = false;

  reader.set_file(f);
  do {
    chr = reader.nextchar();
    while (process() == Action::ProcessChr)
      ;
  } while (chr);
  data.readed_bytes = reader.readed_bytes();
  return Lexer::Result::OK;
}

Lexer::Action Lexer::process()
{
  switch (state) {
    case LexState::ReadingWhitespace:
      switch (chr) {
        case ' ':
        case '\t':
        case '\r':
          // Ignore whitespace
          break;
        case '\n':
          if (prepro) {
            add_token(TokenKind::PPEnd, reader.pos());
            prepro = false;
          }
          break;
        case '\\':
          state = LexState::ReadingWhitespaceToEOL;
          break;
        case '#':
          state = LexState::ReadingIdentifier;
          prepro = true;
          add_token(TokenKind::PPBegin, reader.pos());
          tok_id.clear();
          break;
        case '"':
          state = LexState::ReadingString;
          tok_id.clear();
          break;
        case '\'':
          state = LexState::ReadingChar;
          tok_id.clear();
          break;
        case '{': case '}':
        case '(': case ')':
        case '[': case ']':
        case ',': case ';': case '?':
        case '@':               // Objective-C
          data.add_token(TokenKind::Punctuator, reader.pos(), chr);
          break;
        case '.': {
          int chr2 = reader.nextchar();
          if (chr2 >= '0' && chr2 <= '9') {
            tok_id.push_back(chr);
            tok_id.push_back(chr2);
            state = LexState::ReadingDecimalPart;
          }
          else {
            data.add_token(TokenKind::Punctuator, reader.pos(), chr);
            chr = chr2;
            return Action::ProcessChr;
          }
          break;
        }
        case '+':
          chr = reader.nextchar();
          if (chr == '+') {
            data.add_token(TokenKind::Punctuator, reader.pos(), '+', '+');
          }
          else if (chr == '=') {
            data.add_token(TokenKind::Punctuator, reader.pos(), '+', '=');
          }
          else {
            data.add_token(TokenKind::Punctuator, reader.pos(), '+');
            return Action::ProcessChr;
          }
          break;
        case '-':
          chr = reader.nextchar();
          if (chr == '-') {
            data.add_token(TokenKind::Punctuator, reader.pos(), '-', '-');
          }
          else if (chr == '=') {
            data.add_token(TokenKind::Punctuator, reader.pos(), '-', '=');
          }
          else if (chr == '>') {
            data.add_token(TokenKind::Punctuator, reader.pos(), '-', '>');
          }
          else {
            data.add_token(TokenKind::Punctuator, reader.pos(), '-');
            return Action::ProcessChr;
          }
          break;
        case '/':
          chr = reader.nextchar();
          if (chr == '/') {     // //
            state = LexState::ReadingLineComment;
          }
          else if (chr == '*') { // /*
            state = LexState::ReadingMultilineComment;
          }
          else if (chr == '=') { // /=
            data.add_token(TokenKind::Punctuator, reader.pos(), '/', '=');
          }
          else {
            data.add_token(TokenKind::Punctuator, reader.pos(), '/');
            return Action::ProcessChr;
          }
          break;
        case '&':
        case '|':
        case ':': {
          int chr2 = reader.nextchar();
          if (chr == chr2) {    // && || ::
            add_token(TokenKind::Punctuator, reader.pos(), chr, chr2);
          }
          else {
            add_token(TokenKind::Punctuator, reader.pos(), chr);
            chr = chr2;
            return Action::ProcessChr;
          }
          break;
        }
        case '^':
        case '%':
        case '*':
        case '!': {
          int chr2 = reader.nextchar();
          if (chr2 == '=') {    // ^= %= *= !=
            add_token(TokenKind::Punctuator, reader.pos(), chr, chr2);
          }
          else {                // ^ % * ! ^
            add_token(TokenKind::Punctuator, reader.pos(), chr);
            chr = chr2;
            return Action::ProcessChr;
          }
          break;
        }
        case '<':
        case '>':
        case '=': {
          int chr2 = reader.nextchar();
          if (chr == chr2 ||    // << >> ==
              chr2 == '=') {    // <= >= ==
            data.add_token(TokenKind::Punctuator, reader.pos(), chr, chr2);
          }
          else {
            data.add_token(TokenKind::Punctuator, reader.pos(), chr);
            chr = chr2;
            return Action::ProcessChr;
          }
          break;
        }
        default:
          if ((chr >= 'a' && chr <= 'z') ||
              (chr >= 'A' && chr <= 'Z') ||
              (chr >= '_')) {
            state = LexState::ReadingIdentifier;
            tok_id.push_back(chr);
          }
          // Octal/hexadecimal/binary
          else if (chr == '0') {
            int chr2 = reader.nextchar();
            // Hexadecimal
            if (chr2 == 'x' || chr2 == 'X') {
              state = LexState::ReadingHexadecimal;
              tok_id.push_back(chr);
              tok_id.push_back(chr2);
            }
            // Binary
            else if (chr2 == 'b' || chr2 == 'B') {
              state = LexState::ReadingBinary;
              tok_id.push_back(chr);
              tok_id.push_back(chr2);
            }
            // Octal
            else if (chr2 >= '0' && chr2 <= '7') {
              state = LexState::ReadingOctal;
              tok_id.push_back(chr);
              tok_id.push_back(chr2);
            }
            // Decimal
            else if (chr2 == '.') {
              state = LexState::ReadingDecimalPart;
              tok_id.push_back(chr);
              tok_id.push_back(chr2);
            }
            else {
              tok_id.push_back(chr);
              add_token_id(TokenKind::NumericConstant);
              state = LexState::ReadingWhitespace;
              chr = chr2;
              return Action::ProcessChr;
            }
          }
          // Decimal/double/float
          else if (chr >= '1' && chr <= '9') {
            state = LexState::ReadingIntegerPart;
            tok_id.push_back(chr);
          }
          else if (chr == 0 && reader.eof()) {
            // EOF
            break;
          }
          else {
            error("unexpected char: %d '%c'", chr, chr);
          }
          break;
      }
      break;
    case LexState::ReadingWhitespaceToEOL:
      switch (chr) {
        case ' ':
        case '\t':
        case '\r':
          // Ignore whitespace
          break;
        case '\n':
          state = LexState::ReadingWhitespace;
          break;
        default:
          error("unexpected char '%c' after '\'", chr);
          break;
      }
      break;
    case LexState::ReadingErrorTextToEOL:
      assert(prepro);
      switch (chr) {
        case '\n':
          add_token_id(TokenKind::Literal);
          add_token(TokenKind::PPEnd, reader.pos());
          state = LexState::ReadingWhitespace;
          prepro = false;
          break;
        default:
          tok_id.push_back(chr);
          break;
      }
      break;
    case LexState::ReadingIdentifier:
      if ((chr >= 'a' && chr <= 'z') ||
          (chr >= 'A' && chr <= 'Z') ||
          (chr >= '0' && chr <= '9') ||
          (chr >= '_')) {
        tok_id.push_back(chr);
      }
      else if (prepro) {
        auto it = pp_keywords.find(tok_id);
        if (it != pp_keywords.end()) {
          add_token(TokenKind::PPKeyword, reader.pos(), (int)it->second);
          tok_id.clear();
          switch ((PPKeyword)it->second) {
            case pp_key_include:
              state = LexState::ReadingBeforeHeaderName;
              break;
            case pp_key_error:
              state = LexState::ReadingErrorTextToEOL;
              break;
            default:
              state = LexState::ReadingWhitespace;
              break;
          }
        }
        else {
          add_token_id(TokenKind::Identifier);
          state = LexState::ReadingWhitespace;
        }
        return Action::ProcessChr;
      }
      else {
        auto it = keywords.find(tok_id);
        if (it != keywords.end()) {
          add_token(TokenKind::Keyword, reader.pos(), (int)it->second);
          tok_id.clear();
        }
        else
          add_token_id(TokenKind::Identifier);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
    case LexState::ReadingLineComment:
      if (chr == '\n') {
        if (keep_comments) {
          tok_id.push_back(chr);
          add_token_comment();
        }
        state = LexState::ReadingWhitespace;
      }
      else if (keep_comments) {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingMultilineComment:
      if (chr == '*') {
        chr = reader.nextchar();
        if (chr == '/') {
          if (keep_comments)
            add_token_comment();
          state = LexState::ReadingWhitespace;
        }
        else if (keep_comments) {
          tok_id.push_back(chr);
        }
      }
      else if (keep_comments) {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingBeforeHeaderName:
      switch (chr) {
        case ' ':
        case '\t':
          // Ignore whitespace before the header filename
          break;
        case '<':
          state = LexState::ReadingSysHeaderName;
          tok_id.push_back(chr);
          break;
        case '"':
          state = LexState::ReadingUserHeaderName;
          tok_id.push_back(chr);
          break;
        default:
          // It can be an ID (e.g. #include __SOMETHING__)
          if ((chr >= 'a' && chr <= 'z') ||
              (chr >= 'A' && chr <= 'Z') ||
              (chr >= '_')) {
            state = LexState::ReadingIdentifier;
            tok_id.push_back(chr);
          }
          else {
            error("unexpected char '%c' after #include", chr);
            return Action::ProcessChr;
          }
          break;
      }
      break;
    case LexState::ReadingSysHeaderName:
      if (chr == '>') {
        tok_id.push_back(chr);
        add_token_id(TokenKind::PPHeaderName);
        state = LexState::ReadingWhitespace;
      }
      else if (chr == '\\') {
        chr = reader.nextchar();
        switch (chr) {
          case 'n': tok_id.push_back('\n'); break;
          case 'r': tok_id.push_back('\r'); break;
          case 't': tok_id.push_back('\t'); break;
          default:  tok_id.push_back(chr); break;
        }
      }
      else {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingUserHeaderName:
      if (chr == '"') {
        tok_id.push_back(chr);
        add_token_id(TokenKind::PPHeaderName);
        state = LexState::ReadingWhitespace;
      }
      else if (chr == '\\') {
        chr = reader.nextchar();
        switch (chr) {
          case 'n': tok_id.push_back('\n'); break;
          case 'r': tok_id.push_back('\r'); break;
          case 't': tok_id.push_back('\t'); break;
          default:  tok_id.push_back(chr); break;
        }
      }
      else {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingString:
      if (chr == '"') {
        add_token_id(TokenKind::Literal);
        state = LexState::ReadingWhitespace;
      }
      else if (chr == '\\') {
        chr = reader.nextchar();
        switch (chr) {
          case 'n': tok_id.push_back('\n'); break;
          case 'r': tok_id.push_back('\r'); break;
          case 't': tok_id.push_back('\t'); break;
          default:  tok_id.push_back(chr); break;
        }
      }
      else {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingWideString:
      break;
    case LexState::ReadingChar:
      if (chr == '\'') {
        add_token_id(TokenKind::CharConstant);
        state = LexState::ReadingWhitespace;
      }
      else if (chr == '\\') {
        chr = reader.nextchar();
        switch (chr) {
          case 'n': tok_id.push_back('\n'); break;
          case 'r': tok_id.push_back('\r'); break;
          case 't': tok_id.push_back('\t'); break;
          default:  tok_id.push_back(chr); break;
        }
      }
      else {
        tok_id.push_back(chr);
      }
      break;
    case LexState::ReadingWideChar:
      break;
    case LexState::ReadingHexadecimal:
      if ((chr >= 'a' && chr <= 'f') ||
          (chr >= 'A' && chr <= 'F') ||
          (chr >= '0' && chr <= '0')) {
        tok_id.push_back(chr);
      }
      else {
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
    case LexState::ReadingBinary:
      if (chr == '0' || chr == '1') {
        tok_id.push_back(chr);
      }
      else {
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
    case LexState::ReadingOctal:
      if (chr >= '0' && chr <= '7') {
        tok_id.push_back(chr);
      }
      else if (chr >= '8' && chr <= '9') {
        error("invalid digit '%c' in octal constant", chr);
      }
      else {
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
    case LexState::ReadingIntegerPart:
      if (chr >= '0' && chr <= '9') {
        tok_id.push_back(chr);
      }
      else if (chr == '.') {
        state = LexState::ReadingDecimalPart;
        tok_id.push_back(chr);
      }
      else {
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
    case LexState::ReadingDecimalPart:
      if (chr >= '0' && chr <= '9') {
        tok_id.push_back(chr);
      }
      else if (chr == 'f') {
        tok_id.push_back(chr);  // Indicating f for "float" type instead of double
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        break;
      }
      else {
        add_token_id(TokenKind::NumericConstant);
        state = LexState::ReadingWhitespace;
        return Action::ProcessChr;
      }
      break;
  }
  return Action::NextChr;
}

//////////////////////////////////////////////////////////////////////
// parser

struct ParserData {
  struct Type {
    std::string id;
    TextPos pos;
    Type() { }
  };
  struct Node {
    enum Type {
      Include,
      Func,
      Stmt,
    };
    Type type;
  };
  std::string fn;
  std::vector<Type> types;
  std::vector<Node> tree;
};

class Parser {
public:
  Parser() { }
  void parse(const LexData& lex);

  ParserData&& move_data() { return std::move(data); }

private:
  const Token& goto_token(int i) {
    tok_i = i;
    if (i >= 0 && i < lex_data->tokens.size())
      tok = &lex_data->tokens[i];
    else
      tok = &eof;
    return *tok;
  }
  const Token& next_token() {
    goto_token(++tok_i);
    return *tok;
  }

  bool is(TokenKind kind) const {
    return (tok->kind == kind);
  }

  bool parse_dcl_seq();
  bool parse_dcl();
  bool parse_pp_line();

  ParserData data;
  int tok_i;
  Token eof = { TokenKind::Eof, TextPos(0, 0) };
  const LexData* lex_data;
  const Token* tok;
  int depth = 0;
};

void Parser::parse(const LexData& lex) // TODO
{
  data.fn = lex.fn;

  lex_data = &lex;
  goto_token(0);

  parse_dcl_seq();
}

bool Parser::parse_dcl_seq()
{
  while (next_token().kind != TokenKind::Eof) {
    if (!parse_dcl())
      return false;
  }
  return true;
}

bool Parser::parse_dcl()
{
  if (is(TokenKind::PPBegin)) {
    return parse_pp_line();
  }
  else {
    return false;
  }
}

bool Parser::parse_pp_line()
{
  assert(is(TokenKind::PPBegin));
  next_token();
  if (is(TokenKind::PPKeyword)) {
    switch ((PPKeyword)tok->i) {
      case pp_key_include:
        // TODO add_node();
        break;
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////
// tools

void show_tokens(const LexData& data)
{
  std::printf("%s: tokens=%d\n",
              data.fn.c_str(),
              int(data.tokens.size()));

  for (auto& tok : data.tokens) {
    std::printf("%s:%d:%d: ",
                data.fn.c_str(),
                tok.pos.line, tok.pos.col);
    switch (tok.kind) {
      case TokenKind::PPBegin:
        std::printf("PP { \n");
        break;
      case TokenKind::PPKeyword:
        std::printf("PPKEY %s\n", pp_keywords_id[tok.i].c_str());
        break;
      case TokenKind::PPHeaderName:
        std::printf("PP.H ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::PPEnd:
        std::printf("} PP\n");
        break;
      case TokenKind::Comment:
        std::printf("COMMENT ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.comments[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Identifier:
        std::printf("ID ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Literal:
        std::printf("LIT ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::CharConstant:
        std::printf("CHR ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::NumericConstant:
        std::printf("NUM ");
        for (int i=tok.i; i<tok.j; ++i)
          std::putc(data.ids[i], stdout);
        std::putc('\n', stdout);
        break;
      case TokenKind::Keyword:
        std::printf("KEY %s\n", keywords_id[tok.i].c_str());
        break;
      case TokenKind::Punctuator:
        if (tok.j)
          std::printf("OP %c%c\n", (char)tok.i, (char)tok.j);
        else
          std::printf("OP %c\n", (char)tok.i);
        break;
    }
  }
}

struct PPIf {
  std::string id;
  bool def;
};

void show_includes(const LexData& data)
{
  std::vector<PPIf> stack;

  std::printf("%s: includes\n",
              data.fn.c_str());

  for (int i=0; i+2<int(data.tokens.size()); ++i) {
    if (data.tokens[i].kind == TokenKind::PPBegin &&
        data.tokens[i+1].kind == TokenKind::PPKeyword) {
      if (data.tokens[i+1].i == pp_key_if ||
          data.tokens[i+1].i == pp_key_ifdef ||
          data.tokens[i+1].i == pp_key_ifndef) {
        PPIf ppif;
        switch (data.tokens[i+1].i) {
          case pp_key_if: {
            bool first = false;
            ppif.id = "#if (";
            for (i+=2; i<int(data.tokens.size()) &&
                   data.tokens[i].kind != TokenKind::PPEnd; ++i) {
              if (data.tokens[i].kind == TokenKind::Identifier ||
                  data.tokens[i].kind == TokenKind::Literal) {
                std::string lit(data.ids.begin()+data.tokens[i].i,
                                data.ids.begin()+data.tokens[i].j);
                if (first)
                  first = false;
                else
                  ppif.id.push_back(' ');
                ppif.id += lit;
              }
            }
            ppif.id += ")";
            ppif.def = true;
            break;
          }
          case pp_key_ifndef:
          case pp_key_ifdef: {
            std::string id(data.ids.begin()+data.tokens[i+2].i,
                           data.ids.begin()+data.tokens[i+2].j);
            ppif.id = id;
            ppif.def = (data.tokens[i+1].i == pp_key_ifdef);
            break;
          }
        }
        stack.push_back(ppif);
      }
      else if (data.tokens[i+1].i == pp_key_endif) {
        if (!stack.empty())
          stack.erase(--stack.end());
      }
      else if (data.tokens[i+1].i == pp_key_include &&
               data.tokens[i+2].kind == TokenKind::PPHeaderName) {
        std::string str(data.ids.begin()+data.tokens[i+2].i,
                        data.ids.begin()+data.tokens[i+2].j);
        std::printf("  %s", str.c_str());
        if (!stack.empty()) {
          std::printf(" (");
          for (int i=0; i<int(stack.size()); ++i) {
            if (i > 0)
              std::printf(" && ");
            std::printf("%s%s",
                        stack[i].def ? "": "!",
                        stack[i].id.c_str());
          }
          std::printf(")");
        }
        std::printf("\n");
      }
    }
  }
}

int count_lines(const LexData& data)
{
  int nlines = 0;
  int line = 0;
  for (auto& tok : data.tokens) {
    if (line != tok.pos.line) {
      line = tok.pos.line;
      ++nlines;
    }
  }
  return nlines;
}

//////////////////////////////////////////////////////////////////////
// KeywordStats

class KeywordStats {
  std::array<size_t, MaxKeyword> keywords;
public:
  KeywordStats() { keywords.fill(0); }

  void add(const LexData& data) {
    for (auto& tok : data.tokens) {
      if (tok.kind == TokenKind::Keyword)
        ++keywords[tok.i];
    }
  }

  void print() {
    for (int i=0; i<int(MaxKeyword); ++i) {
      if (keywords[i] > 0)
        std::printf("%d\t%s\n", keywords[i], keywords_id[i].c_str());
    }
  }
};

//////////////////////////////////////////////////////////////////////
// CLI Options

struct Options {
  std::string command;
  std::string print;
  std::vector<std::string> parse_files;
  int threads;
  bool show_time = false;
  bool show_tokens = false;
  bool show_includes = false;
  bool count_tokens = false;
  bool count_lines = false;
  bool keyword_stats = false;
};

//////////////////////////////////////////////////////////////////////
// Commands

#include "docs.h"

//////////////////////////////////////////////////////////////////////
// main

bool parse_options(int argc, char* argv[], Options& options)
{
  for (int i=1; i<argc; ++i) {
    // Ignore empty args?
    if (argv[i][0] != '-') {
      if (options.command.empty())
        options.command = argv[i];
      else
        options.parse_files.push_back(argv[i]);
      continue;
    }
    else if (options.command.empty()) {
      options.command = "none";
    }

    if (std::strcmp(argv[i], "-h") == 0) {
      std::printf("%s [-h] [directory | files.cpp]",
                  argv[0]);
      return false;
    }
    else if (std::strcmp(argv[i], "-filelist") == 0) {
      ++i;
      if (i < argc) {
        std::ifstream f(argv[i]);
        std::string line;
        while (std::getline(f, line)) {
          options.parse_files.push_back(line);
        }
      }
    }
    else if (std::strcmp(argv[i], "-print") == 0) {
      ++i;
      if (i < argc) {
        options.print = argv[i];
      }
    }
    else if (std::strcmp(argv[i], "-showtime") == 0) {
      options.show_time = true;
    }
    else if (std::strcmp(argv[i], "-showtokens") == 0) {
      options.show_tokens = true;
    }
    else if (std::strcmp(argv[i], "-showincludes") == 0) {
      options.show_includes = true;
    }
    else if (std::strcmp(argv[i], "-counttokens") == 0) {
      options.count_tokens = true;
    }
    else if (std::strcmp(argv[i], "-countlines") == 0) {
      options.count_lines = true;
    }
    else if (std::strcmp(argv[i], "-keywordstats") == 0) {
      options.keyword_stats = true;
    }
    else if (std::strcmp(argv[i], "-threads") == 0) {
      ++i;
      if (i < argc) {
        options.threads = std::strtol(argv[i], nullptr, 10);
      }
    }
    else {
      std::printf("%s: invalid argument %s", argv[0], argv[i]);
      return false;
    }
  }
  return true;
}

void run_with_options(const Options& options)
{
  std::printf("running command \"%s\"\n", options.command.c_str());

  Stopwatch t;
  thread_pool pool(options.threads);
  std::vector<LexData> lex_data;
  std::vector<ParserData> parser_data;
  std::mutex lex_mutex;
  std::mutex parser_mutex;

  for (const auto& fn : options.parse_files) {
    pool.execute(
      [&pool, fn,
       &lex_data, &lex_mutex,
       &parser_data, &parser_mutex]{
        Lexer lexer;
        lexer.lex(fn);

        int i;
        {
          std::unique_lock<std::mutex> l(lex_mutex);
          i = lex_data.size();
          lex_data.emplace_back(lexer.move_data());
        }

        pool.execute(
          [i, &lex_data, &lex_mutex,
           &parser_data, &parser_mutex]{
            LexData data;
            {
              std::unique_lock<std::mutex> l(lex_mutex);
              data = lex_data[i];
            }
            Parser parser;
            parser.parse(data);

            std::unique_lock<std::mutex> l(parser_mutex);
            parser_data.emplace_back(parser.move_data());
          });
      });
  }
  pool.wait_all();

  if (options.show_time)
    t.watch("parse files");

  if (options.command == "docs")
    docs::run(options, pool, lex_data, parser_data);

  if (options.count_tokens) {
    int total_tokens = 0;
    for (auto& data : lex_data)
      total_tokens += data.tokens.size();

    std::printf("total tokens %d\n", total_tokens);
  }

  if (options.count_lines) {
    int total_lines = 0;
    for (auto& data : lex_data)
      total_lines += count_lines(data);

    std::printf("total lines %d\n", total_lines);
  }

  if (options.keyword_stats) {
    KeywordStats keyword_stats;
    for (auto& data : lex_data)
      keyword_stats.add(data);

    keyword_stats.print();
  }

  if (options.show_tokens) {
    for (auto& data : lex_data)
      show_tokens(data);
  }

  if (options.show_includes) {
    for (auto& data : lex_data)
      show_includes(data);
  }
}

void create_keyword_tables()
{
  #define PP_KEYWORD(x)           \
    pp_keywords[#x] = pp_key_##x; \
    pp_keywords_id.push_back(#x);
  #define KEYWORD(x)          \
    keywords[#x] = key_##x;   \
    keywords_id.push_back(#x);
  #include "keywords.h"
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::printf("%s: no input file", argv[0]);
    return 1;
  }

  Options options;
  options.threads = std::thread::hardware_concurrency();
  if (!parse_options(argc, argv, options))
    return 0;

  create_keyword_tables();
  run_with_options(options);
  return 0;
}
