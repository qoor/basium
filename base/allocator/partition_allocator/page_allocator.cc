// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"

#include <limits.h>

#include <atomic>

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/base_export.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <mach/mach.h>
#endif

#if defined(OS_POSIX)

#include <errno.h>
#include <sys/mman.h>

#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace base {

namespace {

// On POSIX |mmap| uses a nearby address if the hint address is blocked.
const bool kHintIsAdvisory = true;
std::atomic<int32_t> s_allocPageErrorCode{0};

int GetAccessFlags(PageAccessibilityConfiguration page_accessibility) {
  switch (page_accessibility) {
    case PageReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageReadExecute:
      return PROT_READ | PROT_EXEC;
    default:
      NOTREACHED();
    // Fall through.
    case PageInaccessible:
      return PROT_NONE;
  }
}

#elif defined(OS_WIN)

#include <windows.h>

namespace base {

namespace {

// |VirtualAlloc| will fail if allocation at the hint address is blocked.
const bool kHintIsAdvisory = false;
std::atomic<int32_t> s_allocPageErrorCode{ERROR_SUCCESS};

int GetAccessFlags(PageAccessibilityConfiguration page_accessibility) {
  switch (page_accessibility) {
    case PageReadWrite:
      return PAGE_READWRITE;
    case PageReadExecute:
      return PAGE_EXECUTE_READ;
    default:
      NOTREACHED();
    // Fall through.
    case PageInaccessible:
      return PAGE_NOACCESS;
  }
}

#else
#error Unknown OS
#endif  // defined(OS_POSIX)

// We may reserve / release address space on different threads.
static LazyInstance<subtle::SpinLock>::Leaky s_reserveLock =
    LAZY_INSTANCE_INITIALIZER;
// We only support a single block of reserved address space.
void* s_reservation_address = nullptr;
size_t s_reservation_size = 0;

// This internal function wraps the OS-specific page allocation call:
// |VirtualAlloc| on Windows, and |mmap| on POSIX.
static void* SystemAllocPages(void* hint,
                              size_t length,
                              PageAccessibilityConfiguration page_accessibility,
                              bool commit) {
  DCHECK(!(length & kPageAllocationGranularityOffsetMask));
  DCHECK(!(reinterpret_cast<uintptr_t>(hint) &
           kPageAllocationGranularityOffsetMask));
  DCHECK(commit || page_accessibility == PageInaccessible);

  void* ret;
#if defined(OS_WIN)
  DWORD access_flag = GetAccessFlags(page_accessibility);
  const DWORD type_flags = commit ? (MEM_RESERVE | MEM_COMMIT) : MEM_RESERVE;
  ret = VirtualAlloc(hint, length, type_flags, access_flag);
  if (ret == nullptr)
    s_allocPageErrorCode = GetLastError();
#else

#if defined(OS_MACOSX)
  // Use a custom tag to make it easier to distinguish partition alloc regions
  // in vmmap.
  int fd = VM_MAKE_TAG(254);
#else
  int fd = -1;
#endif
  int access_flag = GetAccessFlags(page_accessibility);
  ret = mmap(hint, length, access_flag, MAP_ANONYMOUS | MAP_PRIVATE, fd, 0);
  if (ret == MAP_FAILED) {
    s_allocPageErrorCode = errno;
    ret = nullptr;
  }
#endif
  return ret;
}

static void* AllocPagesIncludingReserved(
    void* address,
    size_t length,
    PageAccessibilityConfiguration page_accessibility,
    bool commit) {
  void* ret = SystemAllocPages(address, length, page_accessibility, commit);
  if (ret == nullptr) {
    const bool cant_alloc_length = kHintIsAdvisory || address == nullptr;
    if (cant_alloc_length) {
      // The system cannot allocate |length| bytes. Release any reserved address
      // space and try once more.
      ReleaseReservation();
      ret = SystemAllocPages(address, length, page_accessibility, commit);
    }
  }
  return ret;
}

// Trims base to given length and alignment. Windows returns null on failure and
// frees base.
static void* TrimMapping(void* base,
                         size_t base_length,
                         size_t trim_length,
                         uintptr_t align,
                         PageAccessibilityConfiguration page_accessibility,
                         bool commit) {
  size_t pre_slack = reinterpret_cast<uintptr_t>(base) & (align - 1);
  if (pre_slack)
    pre_slack = align - pre_slack;
  size_t post_slack = base_length - pre_slack - trim_length;
  DCHECK(base_length >= trim_length || pre_slack || post_slack);
  DCHECK(pre_slack < base_length);
  DCHECK(post_slack < base_length);
  void* ret = base;

#if defined(OS_POSIX)
  // On POSIX we can resize the allocation run. Release unneeded memory before
  // and after the aligned range.
  (void)page_accessibility;
  if (pre_slack) {
    int res = munmap(base, pre_slack);
    CHECK(!res);
    ret = reinterpret_cast<char*>(base) + pre_slack;
  }
  if (post_slack) {
    int res = munmap(reinterpret_cast<char*>(ret) + trim_length, post_slack);
    CHECK(!res);
  }
#else
  if (pre_slack || post_slack) {
    // On Windows we can't resize the allocation run. Free it and retry at the
    // aligned address within the freed range.
    ret = reinterpret_cast<char*>(base) + pre_slack;
    FreePages(base, base_length);
    ret = SystemAllocPages(ret, trim_length, page_accessibility, commit);
  }
#endif

  return ret;
}

}  // namespace

void* AllocPages(void* address,
                 size_t length,
                 size_t align,
                 PageAccessibilityConfiguration page_accessibility,
                 bool commit) {
  DCHECK(length >= kPageAllocationGranularity);
  DCHECK(!(length & kPageAllocationGranularityOffsetMask));
  DCHECK(align >= kPageAllocationGranularity);
  // Alignment must be power of 2 for masking math to work.
  DCHECK_EQ(align & (align - 1), 0UL);
  DCHECK(!(reinterpret_cast<uintptr_t>(address) &
           kPageAllocationGranularityOffsetMask));
  uintptr_t align_offset_mask = align - 1;
  uintptr_t align_base_mask = ~align_offset_mask;
  DCHECK(!(reinterpret_cast<uintptr_t>(address) & align_offset_mask));

  // If the client passed null as the address, choose a good one.
  if (!address) {
    address = GetRandomPageBase();
    address = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) &
                                      align_base_mask);
  }

  // First try to force an exact-size, aligned allocation from our random base.
  for (int count = 0; count < 3; ++count) {
    void* ret = AllocPagesIncludingReserved(address, length, page_accessibility,
                                            commit);
    if (ret) {
      // If the alignment is to our liking, we're done.
      if (!(reinterpret_cast<uintptr_t>(ret) & align_offset_mask))
        return ret;
      // Free the memory and try again.
      FreePages(ret, length);
#if defined(ARCH_CPU_32_BITS)
      // For small address spaces, try an aligned hint in the free range.
      address = reinterpret_cast<void*>(
          (reinterpret_cast<uintptr_t>(ret) + align) & align_base_mask);
#endif
    } else {
      // |ret| is null; we're OOM when an unhinted allocation fails.
      if (kHintIsAdvisory || address == nullptr)
        return nullptr;
#if defined(ARCH_CPU_32_BITS)
      // On 32-bit systems, let the OS choose the base.
      address = nullptr;
#endif
    }

#if !defined(ARCH_CPU_32_BITS)
    // Keep trying random addresses on systems that have a large address space.
    address = GetRandomPageBase();
    address = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) &
                                      align_base_mask);
#endif
  }

  // Make a larger allocation so we can force alignment.
  size_t try_length = length + (align - kPageAllocationGranularity);
  CHECK(try_length >= length);
  void* ret;

  do {
    // Continue randomizing only on POSIX.
    address = kHintIsAdvisory ? GetRandomPageBase() : nullptr;
    ret = AllocPagesIncludingReserved(address, try_length, page_accessibility,
                                      commit);
    // The retries are for Windows, where a race can steal our mapping on
    // resize.
  } while (ret && (ret = TrimMapping(ret, try_length, length, align,
                                     page_accessibility, commit)) == nullptr);

  return ret;
}

void FreePages(void* address, size_t length) {
  DCHECK(!(reinterpret_cast<uintptr_t>(address) &
           kPageAllocationGranularityOffsetMask));
  DCHECK(!(length & kPageAllocationGranularityOffsetMask));
#if defined(OS_POSIX)
  int ret = munmap(address, length);
  CHECK(!ret);
#else
  BOOL ret = VirtualFree(address, 0, MEM_RELEASE);
  CHECK(ret);
#endif
}

bool SetSystemPagesAccess(void* address,
                          size_t length,
                          PageAccessibilityConfiguration page_accessibility) {
  DCHECK(!(length & kSystemPageOffsetMask));
#if defined(OS_POSIX)
  int access_flag = GetAccessFlags(page_accessibility);
  return !mprotect(address, length, access_flag);
#else
  if (page_accessibility == PageInaccessible) {
    return VirtualFree(address, length, MEM_DECOMMIT) != 0;
  } else {
    DWORD access_flag = GetAccessFlags(page_accessibility);
    return !!VirtualAlloc(address, length, MEM_COMMIT, access_flag);
  }
#endif
}

void DecommitSystemPages(void* address, size_t length) {
  DCHECK_EQ(0UL, length & kSystemPageOffsetMask);
#if defined(OS_POSIX)
  // In POSIX, there is no decommit concept. Discarding is an effective way of
  // implementing the Windows semantics where the OS is allowed to not swap the
  // pages in the region.
  //
  // TODO(ajwong): Also explore setting PageInaccessible to make the protection
  // semantics consistent between Windows and POSIX. This might have a perf cost
  // though as both decommit and recommit would incur an extra syscall.
  // http://crbug.com/766882
  DiscardSystemPages(address, length);
#else
  CHECK(SetSystemPagesAccess(address, length, PageInaccessible));
#endif
}

bool RecommitSystemPages(void* address,
                         size_t length,
                         PageAccessibilityConfiguration page_accessibility) {
  DCHECK_EQ(0UL, length & kSystemPageOffsetMask);
  DCHECK_NE(PageInaccessible, page_accessibility);
#if defined(OS_POSIX)
  // On POSIX systems, read the memory to recommit. This has the correct
  // behavior because the API requires the permissions to be the same as before
  // decommitting and all configurations can read.
  (void)address;
  return true;
#endif
  return SetSystemPagesAccess(address, length, page_accessibility);
}

void DiscardSystemPages(void* address, size_t length) {
  DCHECK_EQ(0UL, length & kSystemPageOffsetMask);
#if defined(OS_POSIX)
#if defined(OS_MACOSX)
  // On macOS, MADV_FREE_REUSABLE has comparable behavior to MADV_FREE, but also
  // marks the pages with the reusable bit, which allows both Activity Monitor
  // and memory-infra to correctly track the pages.
  int ret = madvise(address, length, MADV_FREE_REUSABLE);
#else
  int ret = madvise(address, length, MADV_FREE);
#endif
  if (ret != 0 && errno == EINVAL) {
    // MADV_FREE only works on Linux 4.5+ . If request failed,
    // retry with older MADV_DONTNEED . Note that MADV_FREE
    // being defined at compile time doesn't imply runtime support.
    ret = madvise(address, length, MADV_DONTNEED);
  }
  CHECK(!ret);
#else
  // On Windows discarded pages are not returned to the system immediately and
  // not guaranteed to be zeroed when returned to the application.
  using DiscardVirtualMemoryFunction =
      DWORD(WINAPI*)(PVOID virtualAddress, SIZE_T size);
  static DiscardVirtualMemoryFunction discard_virtual_memory =
      reinterpret_cast<DiscardVirtualMemoryFunction>(-1);
  if (discard_virtual_memory ==
      reinterpret_cast<DiscardVirtualMemoryFunction>(-1))
    discard_virtual_memory =
        reinterpret_cast<DiscardVirtualMemoryFunction>(GetProcAddress(
            GetModuleHandle(L"Kernel32.dll"), "DiscardVirtualMemory"));
  // Use DiscardVirtualMemory when available because it releases faster than
  // MEM_RESET.
  DWORD ret = 1;
  if (discard_virtual_memory)
    ret = discard_virtual_memory(address, length);
  // DiscardVirtualMemory is buggy in Win10 SP0, so fall back to MEM_RESET on
  // failure.
  if (ret) {
    void* ptr = VirtualAlloc(address, length, MEM_RESET, PAGE_READWRITE);
    CHECK(ptr);
  }
#endif
}

bool ReserveAddressSpace(size_t size) {
  // To avoid deadlock, call only SystemAllocPages.
  subtle::SpinLock::Guard guard(s_reserveLock.Get());
  if (s_reservation_address == nullptr) {
    void* mem = SystemAllocPages(nullptr, size, PageInaccessible, false);
    if (mem != nullptr) {
      // We guarantee this alignment when reserving address space.
      DCHECK(!(reinterpret_cast<uintptr_t>(mem) &
               kPageAllocationGranularityOffsetMask));
      s_reservation_address = mem;
      s_reservation_size = size;
      return true;
    }
  }
  return false;
}

void ReleaseReservation() {
  // To avoid deadlock, call only FreePages.
  subtle::SpinLock::Guard guard(s_reserveLock.Get());
  if (s_reservation_address != nullptr) {
    FreePages(s_reservation_address, s_reservation_size);
    s_reservation_address = nullptr;
    s_reservation_size = 0;
  }
}

uint32_t GetAllocPageErrorCode() {
  return s_allocPageErrorCode;
}

}  // namespace base
