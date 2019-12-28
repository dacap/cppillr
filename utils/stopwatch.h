// Copyright (C) 2019  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef STOPWATCH_H_INCLUDED
#define STOPWATCH_H_INCLUDED

#include <chrono>
#include <iostream>

// Outputs the time elapsed since the Stopwatch() ctor or the last
// watch()/reset() call.
class Stopwatch {
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
public:
  Stopwatch() { reset(); }
  void reset() { start = std::chrono::high_resolution_clock::now(); }
  void watch(const char* msg) {
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(ns);
    std::cout << msg << " " << us.count() << " microseconds";
    if (ns.count() < 1000)
      std::cout << " (" << ns.count() << " nanoseconds)";
    std::cout << "\n";
    reset();
  }
};

#endif // STOPWATCH_H_INCLUDED
