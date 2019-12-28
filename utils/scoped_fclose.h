// Copyright (C) 2019  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef SCOPED_FCLOSE_H_INCLUDED
#define SCOPED_FCLOSE_H_INCLUDED

#include <cstdio>

struct Scoped_fclose {
  std::FILE* f;
  Scoped_fclose(std::FILE* f) : f(f) { }
  ~Scoped_fclose() { std::fclose(f); }
};

#endif
