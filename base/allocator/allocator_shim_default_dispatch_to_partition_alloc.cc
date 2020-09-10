// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"
#include "base/allocator/allocator_shim_internals.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bits.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

namespace {

// We would usually make g_root a static local variable, as these are guaranteed
// to be thread-safe in C++11. However this does not work on Windows, as the
// initialization calls into the runtime, which is not prepared to handle it.
//
// To sidestep that, we implement our own equivalent to a local `static
// base::NoDestructor<base::ThreadSafePartitionRoot> root`.
//
// The ingredients are:
// - Placement new to avoid a static constructor, and a static destructor.
// - Double-checked locking to get the same guarantees as a static local
//   variable.

// Lock for double-checked locking.
std::atomic<bool> g_initialization_lock;
std::atomic<base::ThreadSafePartitionRoot*> g_root_;
// Buffer for placement new.
uint8_t g_allocator_buffer[sizeof(base::ThreadSafePartitionRoot)];

base::ThreadSafePartitionRoot& Allocator() {
  // Double-checked locking.
  //
  // The proper way to proceed is:
  //
  // auto* root = load_acquire(g_root);
  // if (!root) {
  //   ScopedLock initialization_lock;
  //   root = load_relaxed(g_root);
  //   if (root)
  //     return root;
  //   new_root = Create new root.
  //   release_store(g_root, new_root);
  // }
  //
  // We don't want to use a base::Lock here, so instead we use the
  // compare-and-exchange on a lock variable, but this provides the same
  // guarantees as a regular lock. The code could be made simpler as we have
  // stricter requirements, but we stick to something close to a regular lock
  // for ease of reading, as none of this is performance-critical anyway.
  //
  // If we boldly assume that initialization will always be single-threaded,
  // then we could remove all these atomic operations, but this seems a bit too
  // bold to try yet. Might be worth revisiting though, since this would remove
  // a memory barrier at each load. We could probably guarantee single-threaded
  // init by adding a static constructor which allocates (and hence triggers
  // initialization before any other thread is created).
  auto* root = g_root_.load(std::memory_order_acquire);
  if (LIKELY(root))
    return *root;

  bool expected = false;
  // Semantically equivalent to base::Lock::Acquire().
  while (!g_initialization_lock.compare_exchange_strong(
      expected, true, std::memory_order_acquire, std::memory_order_acquire)) {
  }

  root = g_root_.load(std::memory_order_relaxed);
  // Someone beat us.
  if (root) {
    // Semantically equivalent to base::Lock::Release().
    g_initialization_lock.store(false, std::memory_order_release);
    return *root;
  }

  auto* new_root = new (g_allocator_buffer)
      base::ThreadSafePartitionRoot(false /* enforce_alignment */);
  g_root_.store(new_root, std::memory_order_release);

  // Semantically equivalent to base::Lock::Release().
  g_initialization_lock.store(false, std::memory_order_release);
  return *new_root;
}

using base::allocator::AllocatorDispatch;

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  return Allocator().AllocFlagsNoHooks(0, size);
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  return Allocator().AllocFlagsNoHooks(base::PartitionAllocZeroFill, n * size);
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  static base::NoDestructor<base::ThreadSafePartitionRoot> aligned_allocator{
      true /* enforce_alignment */};
  return aligned_allocator->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                              alignment, size);
}

void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context) {
  return Allocator().ReallocFlags(base::PartitionAllocNoHooks, address, size,
                                  "");
}

void PartitionFree(const AllocatorDispatch*, void* address, void* context) {
  base::ThreadSafePartitionRoot::FreeNoHooks(address);
}

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  // TODO(lizeb): Returns incorrect values for aligned allocations.
  return base::ThreadSafePartitionRoot::GetAllocatedSize(address);
}

}  // namespace

constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &PartitionMalloc,          /* alloc_function */
    &PartitionCalloc,          /* alloc_zero_initialized_function */
    &PartitionMemalign,        /* alloc_aligned_function */
    &PartitionRealloc,         /* realloc_function */
    &PartitionFree,            /* free_function */
    &PartitionGetSizeEstimate, /* get_size_estimate_function */
    nullptr,                   /* batch_malloc_function */
    nullptr,                   /* batch_free_function */
    nullptr,                   /* free_definite_size_function */
    nullptr,                   /* aligned_malloc_function */
    nullptr,                   /* aligned_realloc_function */
    nullptr,                   /* aligned_free_function */
    nullptr,                   /* next */
};

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

extern "C" {

#if !defined(OS_APPLE)

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#endif  // !defined(OS_APPLE)

#ifdef HAVE_STRUCT_MALLINFO
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  return {};
}
#endif

}  // extern "C"
