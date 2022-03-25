# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.11)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG origin/main
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
endif()
