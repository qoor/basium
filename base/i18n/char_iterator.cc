// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/char_iterator.h"

#include "base/check_op.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/icu/source/common/unicode/utf8.h"

namespace base {
namespace i18n {

// UTF8CharIterator ------------------------------------------------------------

UTF8CharIterator::UTF8CharIterator(base::StringPiece str)
    : str_(str), array_pos_(0), next_pos_(0), char_pos_(0), char_(0) {
  if (!str_.empty())
    U8_NEXT(str_.data(), next_pos_, str_.length(), char_);
}

UTF8CharIterator::~UTF8CharIterator() = default;

bool UTF8CharIterator::Advance() {
  if (array_pos_ >= str_.length())
    return false;

  array_pos_ = next_pos_;
  char_pos_++;
  if (next_pos_ < str_.length())
    U8_NEXT(str_.data(), next_pos_, str_.length(), char_);

  return true;
}

// UTF16CharIterator -----------------------------------------------------------

UTF16CharIterator::UTF16CharIterator(StringPiece16 str)
    : UTF16CharIterator(str, 0) {}

UTF16CharIterator::UTF16CharIterator(UTF16CharIterator&& to_move) = default;

UTF16CharIterator::~UTF16CharIterator() = default;

UTF16CharIterator& UTF16CharIterator::operator=(UTF16CharIterator&& to_move) =
    default;

// static
UTF16CharIterator UTF16CharIterator::LowerBound(StringPiece16 str,
                                                size_t array_index) {
  DCHECK_LE(array_index, str.length());
  U16_SET_CP_START(str.data(), 0, array_index);
  return UTF16CharIterator(str, array_index);
}

// static
UTF16CharIterator UTF16CharIterator::UpperBound(StringPiece16 str,
                                                size_t array_index) {
  DCHECK_LE(array_index, str.length());
  U16_SET_CP_LIMIT(str.data(), 0, array_index, str.length());
  return UTF16CharIterator(str, array_index);
}

int32_t UTF16CharIterator::NextCodePoint() const {
  if (next_pos_ >= str_.length())
    return 0;

  UChar32 c;
  U16_GET(str_.data(), 0, next_pos_, str_.length(), c);
  return c;
}

int32_t UTF16CharIterator::PreviousCodePoint() const {
  if (array_pos_ == 0)
    return 0;

  uint32_t pos = array_pos_;
  UChar32 c;
  U16_PREV(str_.data(), 0, pos, c);
  return c;
}

bool UTF16CharIterator::Advance() {
  if (array_pos_ >= str_.length())
    return false;

  array_pos_ = next_pos_;
  char_offset_++;
  if (next_pos_ < str_.length())
    ReadChar();

  return true;
}

bool UTF16CharIterator::Rewind() {
  if (array_pos_ == 0)
    return false;

  next_pos_ = array_pos_;
  char_offset_--;
  U16_PREV(str_.data(), 0, array_pos_, char_);
  return true;
}

UTF16CharIterator::UTF16CharIterator(StringPiece16 str, size_t initial_pos)
    : str_(str),
      array_pos_(initial_pos),
      next_pos_(initial_pos),
      char_offset_(0),
      char_(0) {
  // This has the side-effect of advancing |next_pos_|.
  if (array_pos_ < str_.length())
    ReadChar();
}

void UTF16CharIterator::ReadChar() {
  // This is actually a huge macro, so is worth having in a separate function.
  U16_NEXT(str_.data(), next_pos_, str_.length(), char_);
}

}  // namespace i18n
}  // namespace base
