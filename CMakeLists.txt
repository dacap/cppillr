# Copyright (C) 2019-2020  David Capello
#
# This file is released under the terms of the MIT license.
# Read LICENSE.txt for more information.

cmake_minimum_required(VERSION 3.15)
project(cppiller)

include_directories(.)
if(UNIX)
  add_definitions(-std=c++11 -Wno-switch -Wno-format)
endif()
add_executable(cppillr cppillr/cppillr.cpp)
if(UNIX AND NOT APPLE)
  target_link_libraries(cppillr pthread)
endif()
