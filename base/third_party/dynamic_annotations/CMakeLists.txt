# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(FALSE) # if (is_nacl)
  add_library(basium_dynamic_annotations INTERFACE dynamic_annotations.h)
else()
  set(SOURCES
    ../valgrind/valgrind.h
    dynamic_annotations.c
    dynamic_annotations.h)
  add_library(basium_dynamic_annotations STATIC ${SOURCES})
  target_include_directories(basium_dynamic_annotations PUBLIC ${BASIUM_COMMON_INCLUDE_DIR})
endif()
