// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines some functions that intentionally do an invalid memory access in
// order to trigger an AddressSanitizer (ASan) error report.

#ifndef BASE_DEBUG_ASAN_INVALID_ACCESS_H_
#define BASE_DEBUG_ASAN_INVALID_ACCESS_H_

#include "base/compiler_specific.h"

namespace base {
namespace debug {

#if defined(ADDRESS_SANITIZER) || defined(SYZYASAN)

// Generates an heap buffer overflow.
NOINLINE void AsanHeapOverflow();

// Generates an heap buffer underflow.
NOINLINE void AsanHeapUnderflow();

// Generates an use after free.
NOINLINE void AsanHeapUseAfterFree();

#endif  // ADDRESS_SANITIZER || SYZYASAN

// The "corrupt-block" and "corrupt-heap" classes of bugs is specific to
// SyzyASan.
#if defined(SYZYASAN)

// Corrupts a memory block and makes sure that the corruption gets detected when
// we try to free this block.
NOINLINE void AsanCorruptHeapBlock();

// Corrupts the heap and makes sure that the corruption gets detected when a
// crash occur.
NOINLINE void AsanCorruptHeap();

#endif  // SYZYASAN

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_ASAN_INVALID_ACCESS_H_
