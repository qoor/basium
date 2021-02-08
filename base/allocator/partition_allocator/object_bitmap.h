// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_

#include <climits>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <tuple>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/bits.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

// Bitmap which tracks beginning of allocated objects. The bitmap can be safely
// accessed from multiple threads, but this doesn't imply visibility on the data
// (i.e. no ordering guaranties, since relaxed atomics are used underneath). The
// bitmap itself must be created inside a page, size and alignment of which are
// specified as template arguments |PageSize| and |PageAlignment|.
// |ObjectAlignment| specifies the minimal alignment of objects that are
// allocated inside a page (serves as the granularity in the bitmap).
template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
class ObjectBitmap final {
  using CellType = uintptr_t;
  static constexpr size_t kBitsPerCell = sizeof(CellType) * CHAR_BIT;
  static constexpr size_t kBitmapSize =
      (PageSize + ((kBitsPerCell * ObjectAlignment) - 1)) /
      (kBitsPerCell * ObjectAlignment);
  static constexpr size_t kPageOffsetMask = PageAlignment - 1;
  static constexpr size_t kPageBaseMask = ~kPageOffsetMask;

 public:
  enum class AccessType : uint8_t {
    kAtomic,
    kNonAtomic,
  };
  static constexpr size_t kPageSize = PageSize;
  static constexpr size_t kPageAlignment = PageAlignment;
  static constexpr size_t kObjectAlignment = ObjectAlignment;
  static constexpr size_t kMaxEntries = kBitmapSize * kBitsPerCell;
  static constexpr uintptr_t kSentinel = 0u;

  inline ObjectBitmap();

  template <AccessType = AccessType::kAtomic>
  ALWAYS_INLINE void SetBit(uintptr_t address);
  template <AccessType = AccessType::kAtomic>
  ALWAYS_INLINE void ClearBit(uintptr_t address);
  template <AccessType = AccessType::kAtomic>
  ALWAYS_INLINE bool CheckBit(uintptr_t address) const;

  // Iterates all objects recorded in the bitmap.
  //
  // The callback is of type
  //   void(Address)
  // and is passed the object address as parameter.
  template <AccessType = AccessType::kAtomic, typename Callback>
  inline void Iterate(Callback) const;

  inline void Clear();

 private:
  std::atomic<CellType>& AsAtomicCell(size_t cell_index) {
    return reinterpret_cast<std::atomic<CellType>&>(bitmap_[cell_index]);
  }
  const std::atomic<CellType>& AsAtomicCell(size_t cell_index) const {
    return reinterpret_cast<const std::atomic<CellType>&>(bitmap_[cell_index]);
  }

  template <AccessType>
  ALWAYS_INLINE CellType LoadCell(size_t cell_index) const;
  ALWAYS_INLINE static constexpr std::pair<size_t, size_t> ObjectIndexAndBit(
      uintptr_t);

  std::array<CellType, kBitmapSize> bitmap_;
};

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
constexpr size_t
    ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::kSentinel;

// The constructor can be omitted, but the Chromium's clang plugin wrongly
// warns that the type is not trivially constructible.
template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
inline ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ObjectBitmap() =
    default;

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
              AccessType access_type>
ALWAYS_INLINE void
ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::SetBit(
    uintptr_t address) {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  if (access_type == AccessType::kNonAtomic) {
    bitmap_[cell_index] |= (static_cast<CellType>(1) << object_bit);
    return;
  }
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_or(static_cast<CellType>(1) << object_bit,
                std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
              AccessType access_type>
ALWAYS_INLINE void
ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ClearBit(
    uintptr_t address) {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  if (access_type == AccessType::kNonAtomic) {
    bitmap_[cell_index] &= ~(static_cast<CellType>(1) << object_bit);
    return;
  }
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_and(~(static_cast<CellType>(1) << object_bit),
                 std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
              AccessType access_type>
ALWAYS_INLINE bool
ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::CheckBit(
    uintptr_t address) const {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  return LoadCell<access_type>(cell_index) &
         (static_cast<CellType>(1) << object_bit);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
              AccessType access_type>
ALWAYS_INLINE
    typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::CellType
    ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::LoadCell(
        size_t cell_index) const {
  if (access_type == AccessType::kNonAtomic)
    return bitmap_[cell_index];
  return AsAtomicCell(cell_index).load(std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
ALWAYS_INLINE constexpr std::pair<size_t, size_t>
ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ObjectIndexAndBit(
    uintptr_t address) {
  const uintptr_t offset_in_page = address & kPageOffsetMask;
  const size_t object_number = offset_in_page / kObjectAlignment;
  const size_t cell_index = object_number / kBitsPerCell;
  PA_DCHECK(kBitmapSize > cell_index);
  const size_t bit = object_number % kBitsPerCell;
  return {cell_index, bit};
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
              AccessType access_type,
          typename Callback>
inline void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::Iterate(
    Callback callback) const {
  // The bitmap (|this|) is allocated inside the page with |kPageAlignment|.
  const uintptr_t base = reinterpret_cast<uintptr_t>(this) & kPageBaseMask;
  for (size_t cell_index = 0; cell_index < kBitmapSize; ++cell_index) {
    CellType value = LoadCell<access_type>(cell_index);
    while (value) {
      const int trailing_zeroes = base::bits::CountTrailingZeroBits(value);
      const size_t object_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const uintptr_t object_address =
          base + (kObjectAlignment * object_number);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= ~(static_cast<CellType>(1) << trailing_zeroes);
    }
  }
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::Clear() {
  std::fill(bitmap_.begin(), bitmap_.end(), '\0');
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_
