// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_space_randomization.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/bit_cast.h"
#include "base/bits.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/windows_version.h"
// VersionHelpers.h must be included after windows.h.
#include <VersionHelpers.h>
#endif

namespace base {

TEST(AddressSpaceRandomizationTest, GetRandomPageBase) {
  uintptr_t mask = internal::kASLRMask;
#if defined(ARCH_CPU_64_BITS) && defined(OS_WIN)
  if (!IsWindows8Point1OrGreater()) {
    mask = internal::kASLRMaskBefore8_10;
  }
#endif
  // Sample the first 100 addresses.
  std::set<uintptr_t> addresses;
  uintptr_t address_logical_sum = 0;
  uintptr_t address_logical_product = static_cast<uintptr_t>(-1);
  for (int i = 0; i < 100; i++) {
    uintptr_t address = reinterpret_cast<uintptr_t>(base::GetRandomPageBase());
    // Test that address is in range.
    EXPECT_LE(internal::kASLROffset, address);
    EXPECT_GE(internal::kASLROffset + mask, address);
    // Test that address is page aligned.
    EXPECT_EQ(0ULL, (address & kPageAllocationGranularityOffsetMask));
    // Test that address is unique (no collisions in 100 tries)
    CHECK_EQ(0ULL, addresses.count(address));
    addresses.insert(address);
    // Sum and product to test randomness at each bit position, below.
    address -= internal::kASLROffset;
    address_logical_sum |= address;
    address_logical_product &= address;
  }
  // All bits in address_logical_sum should be set, since the likelihood of
  // never setting any of the bits is 1 / (2 ^ 100) with a good RNG. Likewise,
  // all bits in address_logical_product should be cleared.
  EXPECT_EQ(mask, address_logical_sum);
  EXPECT_EQ(0ULL, address_logical_product);
}

}  // namespace base
