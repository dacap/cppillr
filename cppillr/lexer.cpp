// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "cppillr/lexer.h"
#include "utils/scoped_fclose.h"
#include "utils/string.h"

#include <cassert>

int CharReader::nextchar()
{
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

Lexer::Result Lexer::lex(const std::string& fn)
{
  std::FILE* f = (fn.empty() ? stdin: std::fopen(fn.c_str(), "r"));
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
  data.add_token(TokenKind::Eof, reader.pos());
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
        case '!':
        case '~': {
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
          (chr >= '0' && chr <= '9')) {
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

void Lexer::add_token_id(TokenKind tokenKind)
{
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

void Lexer::add_token_comment()
{
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
