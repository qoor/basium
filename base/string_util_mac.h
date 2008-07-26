// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_STRING_UTIL_MAC_H__
#define BASE_STRING_UTIL_MAC_H__

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

inline bool StrCpy(char* dst, const char* src, size_t dst_size) {
  return strlcpy(dst, src, dst_size) < dst_size;
}

inline int StrNCaseCmp(const char* string1, const char* string2, size_t count) {
  return strncasecmp(string1, string2, count);
}

inline int VSNPrintF(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  return vsnprintf(buffer, size, format, arguments);
}

inline bool WcsCpy(char* dst, const char* src, size_t dst_size) {
  return strlcpy(dst, src, dst_size) < dst_size;
}

inline int VSWPrintF(wchar_t* buffer, size_t size,
                     const wchar_t* format, va_list arguments) {
  return vswprintf(buffer, size, format, arguments);
}

// StrNCpy and WcsNCpy are not inline, so they're implemented in the .cc file.

#endif  // BASE_STRING_UTIL_MAC_H__
