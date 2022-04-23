set(SOURCES
  double-conversion/bignum-dtoa.cc
  double-conversion/bignum-dtoa.h
  double-conversion/bignum.cc
  double-conversion/bignum.h
  double-conversion/cached-powers.cc
  double-conversion/cached-powers.h
  double-conversion/diy-fp.h
  double-conversion/double-conversion.h
  double-conversion/double-to-string.cc
  double-conversion/double-to-string.h
  double-conversion/fast-dtoa.cc
  double-conversion/fast-dtoa.h
  double-conversion/fixed-dtoa.cc
  double-conversion/fixed-dtoa.h
  double-conversion/ieee.h
  double-conversion/string-to-double.cc
  double-conversion/string-to-double.h
  double-conversion/strtod.cc
  double-conversion/strtod.h
  double-conversion/utils.h)
add_library(basium_double_conversion STATIC ${SOURCES})
target_include_directories(basium_double_conversion PUBLIC ${BASIUM_COMMON_INCLUDE_DIR})
