// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum PPKeyword {
  #define PP_KEYWORD(x) pp_key_##x,
  #include "cppillr/keywords_list.h"
  MaxPPKeyword
};

enum Keyword {
  #define KEYWORD(x) key_##x,
  #include "cppillr/keywords_list.h"
  MaxKeyword
};

extern std::unordered_map<std::string, PPKeyword> pp_keywords;
extern std::unordered_map<std::string, Keyword> keywords;
extern std::vector<std::string> pp_keywords_id, keywords_id;

void create_keyword_tables();
