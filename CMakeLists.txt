# Copyright (C) 2019  David Capello
#
# This file is released under the terms of the MIT license.
# Read LICENSE.txt for more information.

include_directories(.)
add_definitions(-std=c++11)
add_executable(cppillr cppillr/cppillr.cpp)
if(UNIX AND NOT APPLE)
  target_link_libraries(cppillr pthread)
endif()
