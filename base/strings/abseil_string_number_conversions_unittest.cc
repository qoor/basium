// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_number_conversions.h"

#include <stdint.h>

#include <limits>

#include "base/cxx17_backports.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace base {

TEST(AbseilStringNumberConversionsTest, HexStringToUInt128) {
  // Test cases adapted from `StringNumberConversionsTest.HexStringToUint64`.
  static const struct {
    std::string input;
    absl::uint128 output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 66, true},
      {"-42", 0, false},
      {"+42", 66, true},
      {"ffffffffffffffff",
       absl::MakeUint128(/*high=*/0,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       true},
      {"1ffffffffffffffff",
       absl::MakeUint128(/*high=*/1,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       true},
      {"7fffffff", INT_MAX, true},
      {"-80000000", 0, false},
      {"ffffffff", 0xffffffff, true},
      {"DeadBeef", 0xdeadbeef, true},
      {"0x42", 66, true},
      {"-0x42", 0, false},
      {"+0x42", 66, true},
      {"0xffffffffffffffff",
       absl::MakeUint128(/*high=*/0,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       true},
      {"0x1ffffffffffffffff",
       absl::MakeUint128(/*high=*/1,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       true},
      {"0x7fffffff", INT_MAX, true},
      {"-0x80000000", 0, false},
      {"0xffffffff", 0xffffffff, true},
      {"0XDeadBeef", 0xdeadbeef, true},
      {"0x7fffffffffffffffffffffffffffffff",
       std::numeric_limits<absl::int128>::max(), true},
      {"-0x8000000000000000", 0, false},
      {"0x8000000000000000",
       absl::MakeUint128(/*high=*/0, UINT64_C(0x8000000000000000)), true},
      {"-0x8000000000000001", 0, false},
      {"0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
       std::numeric_limits<absl::uint128>::max(), true},
      {"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
       std::numeric_limits<absl::uint128>::max(), true},
      {"0x0000000000000000", 0, true},
      {"0000000000000000", 0, true},
      {"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
       std::numeric_limits<absl::uint128>::max(), false},  // Overflow test.
      {"0x0f", 15, true},
      {"0f", 15, true},
      {" 45", 0x45, false},
      {"\t\n\v\f\r 0x45", 0x45, false},
      {" 45", 0x45, false},
      {"45 ", 0x45, false},
      {"45:", 0x45, false},
      {"efgh", 0xef, false},
      {"0xefgh", 0xef, false},
      {"hgfe", 0, false},
      {"-", 0, false},
      {"", 0, false},
      {"0x", 0, false},
  };

  for (const auto& i : cases) {
    absl::uint128 output = 0;
    EXPECT_EQ(i.success, HexStringToUInt128(i.input, &output)) << i.input;
    EXPECT_EQ(i.output, output) << i.input;
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "0xc0ffee\0"
      "9";
  std::string input_string(input, base::size(input) - 1);
  absl::uint128 output;
  EXPECT_FALSE(HexStringToUInt128(input_string, &output));
  EXPECT_EQ(0xc0ffeeU, output);
}

}  // namespace base
