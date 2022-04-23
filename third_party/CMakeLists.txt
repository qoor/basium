# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(NOT TARGET absl::base)
  add_subdirectory(abseil-cpp)
endif()

add_subdirectory(googletest)
add_subdirectory(openssl)
add_subdirectory(modp_b64)
add_subdirectory(icu)
add_subdirectory(ced)
