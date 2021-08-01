// Copyright (C) 2019-2021  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED
#pragma once

#include <string>

void trim_string(std::string& s);
void replace_string(
  std::string& subject,
  const std::string& replace_this,
  const std::string& with_that);

#endif
