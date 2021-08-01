// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "cppillr/keywords.h"

std::unordered_map<std::string, PPKeyword> pp_keywords;
std::unordered_map<std::string, Keyword> keywords;
std::vector<std::string> pp_keywords_id, keywords_id;

void create_keyword_tables()
{
  #define PP_KEYWORD(x)           \
    pp_keywords[#x] = pp_key_##x; \
    pp_keywords_id.push_back(#x);
  #define KEYWORD(x)          \
    keywords[#x] = key_##x;   \
    keywords_id.push_back(#x);
  #include "cppillr/keywords_list.h"
}
