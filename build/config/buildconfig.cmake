# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CLANG ON)
else()
  set(CLANG OFF)
endif()

set(CHROMEOS OFF)
set(LINUX OFF)
set(MAC OFF)

if(NOT ANDROID AND NOT IOS)
  if(CMAKE_SYSTEM_NAME STREQUAL "chromeos")
    set(CHROMEOS ON)
  elseif(CMAKE_SYSTEM_TYPE STREQUAL "Linux")
    set(LINUX ON)
  elseif(APPLE)
    set(MAC ON)
  endif()
endif()
