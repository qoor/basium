// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_UNSAFE_SHARED_MEMORY_POOL_H_
#define BASE_MEMORY_UNSAFE_SHARED_MEMORY_POOL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"

namespace base {

// UnsafeSharedMemoryPool manages allocation and pooling of
// UnsafeSharedMemoryRegions. Using pool saves cost of repeated shared memory
// allocations. Up-to 32 regions would be pooled. It is thread-safe. May return
// bigger regions than requested. If a requested size is increased, all stored
// regions are purged. Regions are returned to the buffer on destruction of
// |SharedMemoryHandle| if they are of a correct size.
class BASE_EXPORT UnsafeSharedMemoryPool
    : public base::RefCountedThreadSafe<UnsafeSharedMemoryPool> {
 public:
  // Used to store the allocation result.
  // This class returns memory to the pool upon destruction.
  class BASE_EXPORT Handle {
   public:
    Handle(base::UnsafeSharedMemoryRegion region,
           base::WritableSharedMemoryMapping mapping,
           scoped_refptr<UnsafeSharedMemoryPool> pool);
    ~Handle();
    // Disallow copy and assign.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    const base::UnsafeSharedMemoryRegion& GetRegion() const;

    const base::WritableSharedMemoryMapping& GetMapping() const;

   private:
    base::UnsafeSharedMemoryRegion region_;
    base::WritableSharedMemoryMapping mapping_;
    scoped_refptr<UnsafeSharedMemoryPool> pool_;
  };

  UnsafeSharedMemoryPool();
  // Disallow copy and assign.
  UnsafeSharedMemoryPool(const UnsafeSharedMemoryPool&) = delete;
  UnsafeSharedMemoryPool& operator=(const UnsafeSharedMemoryPool&) = delete;

  // Allocates a region of the given |size| or reuses a previous allocation if
  // possible.
  std::unique_ptr<Handle> MaybeAllocateBuffer(size_t size);

  // Shuts down the pool, freeing all currently unused allocations and freeing
  // outstanding ones as they are returned.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<UnsafeSharedMemoryPool>;
  ~UnsafeSharedMemoryPool();

  void ReleaseBuffer(base::UnsafeSharedMemoryRegion region,
                     base::WritableSharedMemoryMapping mapping);

  base::Lock lock_;
  // All shared memory regions cached internally are guaranteed to be
  // at least `region_size_` bytes in size.
  size_t region_size_ GUARDED_BY(lock_) = 0u;
  // Cached unused regions and their mappings.
  std::vector<std::pair<base::UnsafeSharedMemoryRegion,
                        base::WritableSharedMemoryMapping>>
      regions_ GUARDED_BY(lock_);
  bool is_shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace base

#endif  // BASE_MEMORY_UNSAFE_SHARED_MEMORY_POOL_H_
