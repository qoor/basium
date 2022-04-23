# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(SOURCES
  checked_math_impl.h
  clamped_math_impl.h
  safe_conversions_arm_impl.h
  safe_conversions_impl.h
  safe_math_arm_impl.h
  safe_math_clang_gcc_impl.h
  safe_math_shared_impl.h)
set(PUBLIC
  checked_math.h
  clamped_math.h
  math_constants.h
  ostream_operators.h
  ranges.h
  safe_conversions.h
  safe_math.h)
add_library(basium_base_numerics INTERFACE ${SOURCES} ${PUBLIC})
target_include_directories(basium_base_numerics INTERFACE ${BASIUM_COMMON_INCLUDE_DIR})
