// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRCAT_INTERNAL_H_
#define BASE_STRINGS_STRCAT_INTERNAL_H_

#include <string>

#include "base/containers/span.h"

namespace base {

namespace internal {

// Reserves an additional amount of capacity in the given string, growing by at
// least 2x if necessary. Used by StrAppendT().
//
// The "at least 2x" growing rule duplicates the exponential growth of
// std::string. The problem is that most implementations of reserve() will grow
// exactly to the requested amount instead of exponentially growing like would
// happen when appending normally. If we didn't do this, an append after the
// call to StrAppend() would definitely cause a reallocation, and loops with
// StrAppend() calls would have O(n^2) complexity to execute. Instead, we want
// StrAppend() to have the same semantics as std::string::append().
template <typename String>
void ReserveAdditionalIfNeeded(String* str,
                               typename String::size_type additional) {
  const size_t required = str->size() + additional;
  // Check whether we need to reserve additional capacity at all.
  if (required <= str->capacity())
    return;

  str->reserve(std::max(required, str->capacity() * 2));
}

template <typename DestString, typename InputString>
void StrAppendT(DestString* dest, span<const InputString> pieces) {
  size_t additional_size = 0;
  for (const auto& cur : pieces)
    additional_size += cur.size();
  ReserveAdditionalIfNeeded(dest, additional_size);

  for (const auto& cur : pieces)
    dest->append(cur.data(), cur.size());
}

template <typename StringT>
auto StrCatT(span<const StringT> pieces) {
  std::basic_string<typename StringT::value_type, typename StringT::traits_type>
      result;
  StrAppendT(&result, pieces);
  return result;
}

}  // namespace internal

}  // namespace base

#endif  // BASE_STRINGS_STRCAT_INTERNAL_H_
