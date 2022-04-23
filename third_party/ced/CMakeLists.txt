# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(ced_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(FetchContent)
FetchContent_Declare(
  ced
  GIT_REPOSITORY https://github.com/google/compact_enc_det.git
  GIT_TAG ba412eaaacd3186085babcd901679a48863c7dd5
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

FetchContent_GetProperties(ced)
if(NOT ced_POPULATED)
  FetchContent_Populate(ced)
endif()

add_library(ced STATIC
    # find src -maxdepth 3 ! -type d | egrep '\.(h|cc)$' | grep -v test.cc |\
    # LC_COLLATE=c sort | sed 's/^\(.*\)$/    "\1",/'
    src/compact_enc_det/compact_enc_det.cc
    src/compact_enc_det/compact_enc_det_generated_tables.h
    src/compact_enc_det/compact_enc_det_generated_tables2.h
    src/compact_enc_det/compact_enc_det_hint_code.cc
    src/compact_enc_det/compact_enc_det_hint_code.h
    src/util/basictypes.h
    src/util/case_insensitive_hash.h
    src/util/commandlineflags.h
    src/util/encodings/encodings.cc
    src/util/encodings/encodings.h
    src/util/encodings/encodings.pb.h
    src/util/languages/languages.cc
    src/util/languages/languages.h
    src/util/languages/languages.pb.h
    src/util/logging.h
    src/util/port.h
    src/util/string_util.h
    src/util/varsetter.h)
target_compile_options(ced PRIVATE -Wno-unused-function -Wno-c++11-narrowing)

target_include_directories(ced PUBLIC src)

if(WIN32)
  target_compile_definitions(ced PRIVATE COMPILER_MSVC)
  target_compile_options(ced PRIVATE
    /wd4005
    /wd4005
    /wd4018
    /wd4309)
else()
  target_compile_definitions(ced PRIVATE COMPILER_GCC)
endif()

target_compile_definitions(ced PRIVATE HTML5_MODE)
