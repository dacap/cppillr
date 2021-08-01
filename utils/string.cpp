// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "utils/string.h"

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
