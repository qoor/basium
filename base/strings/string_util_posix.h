// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_POSIX_H_
#define BASE_STRINGS_STRING_UTIL_POSIX_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "base/check.h"
#include "base/strings/string_piece.h"

namespace base {

// Chromium code style is to not use malloc'd strings; this is only for use
// for interaction with APIs that require it.
inline char* strdup(const char* str) {
  return ::strdup(str);
}

inline int vsnprintf(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  return ::vsnprintf(buffer, size, format, arguments);
}

inline int vswprintf(wchar_t* buffer, size_t size,
                     const wchar_t* format, va_list arguments) {
  DCHECK(IsWprintfFormatPortable(format));
  return ::vswprintf(buffer, size, format, arguments);
}

// These mirror the APIs in string_util_win.h. Since base::StringPiece is
// already the native string type on POSIX platforms these APIs are simple
// no-ops.
inline StringPiece AsCrossPlatformPiece(StringPiece str) {
  return str;
}

inline StringPiece AsNativeStringPiece(StringPiece str) {
  return str;
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_UTIL_POSIX_H_
