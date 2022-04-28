# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${PROJECT_SOURCE_DIR}/build/config/compiler/compiler.cmake)

if(ASAN OR LSAN OR MSAN OR TSAN)
  set(PRINT_UNSYMBOLIZED_STACK_TRACES ON)
else()
  set(PRINT_UNSYMBOLIZED_STACK_TRACES OFF)
endif()

add_library(basium_symbolize STATIC
  config.h
  demangle.cc
  demangle.h
  glog/logging.h
  glog/raw_logging.h
  symbolize.cc
  symbolize.h
  utilities.h)
if(PRINT_UNSYMBOLIZED_STACK_TRACES)
  target_compile_definitions(basium_symbolize PRIVATE PRINT_UNSYMBOLIZED_STACK_TRACES)
endif()
