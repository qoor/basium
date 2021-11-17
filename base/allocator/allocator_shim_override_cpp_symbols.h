// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_CPP_SYMBOLS_H_

// Preempt the default new/delete C++ symbols so they call the shim entry
// points. This file is strongly inspired by tcmalloc's
// libc_override_redefine.h.

#include <new>

#include "base/allocator/allocator_shim_internals.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "build/build_config.h"

// std::align_val_t isn't available until C++17, but we want to override aligned
// new/delete anyway to prevent a possible situation where a library gets loaded
// in that uses the aligned operators.  We want to avoid a situation where
// separate heaps are used.
// TODO(thomasanderson): Remove this once building with C++17 or later.
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606
#define ALIGN_VAL_T std::align_val_t
#define ALIGN_LINKAGE
#define ALIGN_NEW operator new
#define ALIGN_NEW_NOTHROW operator new
#define ALIGN_DEL operator delete
#define ALIGN_DEL_SIZED operator delete
#define ALIGN_DEL_NOTHROW operator delete
#define ALIGN_NEW_ARR operator new[]
#define ALIGN_NEW_ARR_NOTHROW operator new[]
#define ALIGN_DEL_ARR operator delete[]
#define ALIGN_DEL_ARR_SIZED operator delete[]
#define ALIGN_DEL_ARR_NOTHROW operator delete[]
#else
#define ALIGN_VAL_T size_t
#define ALIGN_LINKAGE extern "C"
#if defined(OS_WIN)
#error "Mangling is different on these platforms."
#else
#define ALIGN_NEW _ZnwmSt11align_val_t
#define ALIGN_NEW_NOTHROW _ZnwmSt11align_val_tRKSt9nothrow_t
#define ALIGN_DEL _ZdlPvSt11align_val_t
#define ALIGN_DEL_SIZED _ZdlPvmSt11align_val_t
#define ALIGN_DEL_NOTHROW _ZdlPvSt11align_val_tRKSt9nothrow_t
#define ALIGN_NEW_ARR _ZnamSt11align_val_t
#define ALIGN_NEW_ARR_NOTHROW _ZnamSt11align_val_tRKSt9nothrow_t
#define ALIGN_DEL_ARR _ZdaPvSt11align_val_t
#define ALIGN_DEL_ARR_SIZED _ZdaPvmSt11align_val_t
#define ALIGN_DEL_ARR_NOTHROW _ZdaPvSt11align_val_tRKSt9nothrow_t
#endif
#endif

#if !defined(OS_APPLE)
#define SHIM_CPP_SYMBOLS_EXPORT SHIM_ALWAYS_EXPORT
#else
// On Apple OSes, prefer not exporting these symbols (as this reverts to the
// default behavior, they are still exported in e.g. component builds). This is
// partly due to intentional limits on exported symbols in the main library, but
// it is also needless, since no library used on macOS imports these.
//
// TODO(lizeb): It may not be necessary anywhere to export these.
#define SHIM_CPP_SYMBOLS_EXPORT NOINLINE
#endif

SHIM_CPP_SYMBOLS_EXPORT void* operator new(size_t size) {
  return ShimCppNew(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](size_t size) {
#if defined(OS_APPLE)
  // On macOS builds with Identical Code Folding (ICF), this gets merged with
  // operator new() above, as the functions are identical. This then bumps into
  // the framework.order check. Make sure this function is unique.
  //
  // TODO(lizeb): This function should not be exported at all, investigate why
  // this is the case, and remove this hack.
  NO_CODE_FOLDING();
#endif  // defined(OS_APPLE)
  return ShimCppNew(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p) __THROW {
#if defined(OS_APPLE)
  // See comment above in operator new[]().
  NO_CODE_FOLDING();
#endif  // defined(OS_APPLE)
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new(size_t size,
                                           const std::nothrow_t&) __THROW {
  return ShimCppNewNoThrow(size);
}

SHIM_CPP_SYMBOLS_EXPORT void* operator new[](size_t size,
                                             const std::nothrow_t&) __THROW {
  return ShimCppNewNoThrow(size);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p,
                                             const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p,
                                               const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete(void* p, size_t) __THROW {
  ShimCppDelete(p);
}

SHIM_CPP_SYMBOLS_EXPORT void operator delete[](void* p, size_t) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void* ALIGN_NEW(std::size_t size,
                                                      ALIGN_VAL_T alignment) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void* ALIGN_NEW_NOTHROW(
    std::size_t size,
    ALIGN_VAL_T alignment,
    const std::nothrow_t&) __THROW {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void ALIGN_DEL(void* p,
                                                     ALIGN_VAL_T) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void
ALIGN_DEL_SIZED(void* p, std::size_t size, ALIGN_VAL_T) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void
ALIGN_DEL_NOTHROW(void* p, ALIGN_VAL_T, const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void* ALIGN_NEW_ARR(
    std::size_t size,
    ALIGN_VAL_T alignment) {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void* ALIGN_NEW_ARR_NOTHROW(
    std::size_t size,
    ALIGN_VAL_T alignment,
    const std::nothrow_t&) __THROW {
  return ShimCppAlignedNew(size, static_cast<size_t>(alignment));
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void ALIGN_DEL_ARR(void* p,
                                                         ALIGN_VAL_T) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void
ALIGN_DEL_ARR_SIZED(void* p, std::size_t size, ALIGN_VAL_T) __THROW {
  ShimCppDelete(p);
}

ALIGN_LINKAGE SHIM_CPP_SYMBOLS_EXPORT void
ALIGN_DEL_ARR_NOTHROW(void* p, ALIGN_VAL_T, const std::nothrow_t&) __THROW {
  ShimCppDelete(p);
}
