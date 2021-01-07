// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if defined(OS_WIN)
// VersionHelpers.h depends on Windows.h.
#include <Windows.h>
// For IsWindows8Point1OrGreater().
#include <VersionHelpers.h>
#endif

namespace base {

struct Feature;

namespace features {

extern const BASE_EXPORT Feature kPartitionAllocGigaCage;
extern const BASE_EXPORT Feature kPartitionAllocPCScan;
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
extern const BASE_EXPORT Feature kPartitionAllocPCScanBrowserOnly;
#endif

ALWAYS_INLINE bool IsPartitionAllocGigaCageEnabled() {
#if defined(PA_HAS_64_BITS_POINTERS) && defined(OS_WIN)
  // Lots of crashes (at PartitionAddressSpace::Init) occur
  // when enabling GigaCage on Windows whose version is smaller than 8.1,
  // because PTEs for reserved memory counts against commit limit. See
  // https://crbug.com/1101421.
  // TODO(tasak): this windows version check is the same as GetRandomPageBase()
  // (address_space_randomization.cc). Refactor the code to avoid the
  // duplication.
  static bool is_windows_version_checked = false;
  // Don't assign directly IsWindows8Point1OrGreater() to a static local
  // variable, because the initial value is not trivial and the assignment needs
  // thread-safe static-local initializer on Windows. (i.e. Init_thread_header)
  // This causes issues when used on the allocation path (see
  // crbug.com/1126432). As we don't use atomics here, this may end up querying
  // the version multiple times, which is fine, as this operation is idempotent,
  // with no side-effects.
  static bool recent_enough_windows_version = false;
  if (!is_windows_version_checked) {
    recent_enough_windows_version = IsWindows8Point1OrGreater();
    is_windows_version_checked = true;
  }
  if (!recent_enough_windows_version)
    return false;
#endif  // defined(PA_HAS_64_BITS_POINTERS) && defined(OS_WIN)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return true;
#else
  return FeatureList::IsEnabled(kPartitionAllocGigaCage);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

ALWAYS_INLINE bool IsPartitionAllocPCScanEnabled() {
#if defined(PA_HAS_64_BITS_POINTERS) && !ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR
  return FeatureList::IsEnabled(kPartitionAllocPCScan);
#else  // PA_HAS_64_BITS_POINTERS
  return false;
#endif
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
ALWAYS_INLINE bool IsPartitionAllocPCScanBrowserOnlyEnabled() {
#if defined(PA_HAS_64_BITS_POINTERS) && !ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR
  return FeatureList::IsEnabled(kPartitionAllocPCScanBrowserOnly);
#else  // PA_HAS_64_BITS_POINTERS
  return false;
#endif
}
#endif

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
