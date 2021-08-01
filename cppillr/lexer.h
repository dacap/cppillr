// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include "cppillr/keywords.h"

#include <array>
#include <string>
#include <vector>

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

  int nextchar();

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

  void add_token_id(TokenKind tokenKind);
  void add_token_comment();

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
