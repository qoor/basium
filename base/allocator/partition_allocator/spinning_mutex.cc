// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/spinning_mutex.h"

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_POSIX)
#include <pthread.h>
#endif

#if defined(PA_HAS_LINUX_KERNEL)
#include <errno.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif  // defined(PA_HAS_LINUX_KERNEL)

#if !defined(PA_HAS_FAST_MUTEX)
#include "base/threading/platform_thread.h"

#if BUILDFLAG(IS_POSIX)
#include <sched.h>

#define PA_YIELD_THREAD sched_yield()

#else  // Other OS

#warning "Thread yield not supported on this OS."
#define PA_YIELD_THREAD ((void)0)
#endif

#endif  // !defined(PA_HAS_FAST_MUTEX)

namespace partition_alloc {

void SpinningMutex::Reinit() {
#if !BUILDFLAG(IS_APPLE)
  // On most platforms, no need to re-init the lock, can just unlock it.
  Release();
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

  if (LIKELY(os_unfair_lock_trylock)) {
    unfair_lock_ = OS_UNFAIR_LOCK_INIT;
    return;
  }

#pragma clang diagnostic pop

  Release();
#endif  // BUILDFLAG(IS_APPLE)
}

#if defined(PA_HAS_FAST_MUTEX)

#if defined(PA_HAS_LINUX_KERNEL)

void SpinningMutex::FutexWait() {
  // Save and restore errno.
  int saved_errno = errno;
  // Don't check the return value, as we will not be awaken by a timeout, since
  // none is specified.
  //
  // Ignoring the return value doesn't impact correctness, as this acts as an
  // immediate wakeup. For completeness, the possible errors for FUTEX_WAIT are:
  // - EACCES: state_ is not readable. Should not happen.
  // - EAGAIN: the value is not as expected, that is not |kLockedContended|, in
  //           which case retrying the loop is the right behavior.
  // - EINTR: signal, looping is the right behavior.
  // - EINVAL: invalid argument.
  //
  // Note: not checking the return value is the approach used in bionic and
  // glibc as well.
  //
  // Will return immediately if |state_| is no longer equal to
  // |kLockedContended|. Otherwise, sleeps and wakes up when |state_| may not be
  // |kLockedContended| anymore. Note that even without spurious wakeups, the
  // value of |state_| is not guaranteed when this returns, as another thread
  // may get the lock before we get to run.
  int err = syscall(SYS_futex, &state_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
                    kLockedContended, nullptr, nullptr, 0);

  if (err) {
    // These are programming error, check them.
    PA_DCHECK(errno != EACCES);
    PA_DCHECK(errno != EINVAL);
  }
  errno = saved_errno;
}

void SpinningMutex::FutexWake() {
  int saved_errno = errno;
  long retval = syscall(SYS_futex, &state_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                        1 /* wake up a single waiter */, nullptr, nullptr, 0);
  PA_CHECK(retval != -1);
  errno = saved_errno;
}

void SpinningMutex::LockSlow() {
  // If this thread gets awaken but another one got the lock first, then go back
  // to sleeping. See comments in |FutexWait()| to see why a loop is required.
  while (state_.exchange(kLockedContended, std::memory_order_acquire) !=
         kUnlocked) {
    FutexWait();
  }
}

#elif BUILDFLAG(IS_WIN)

void SpinningMutex::LockSlow() {
  ::AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#elif BUILDFLAG(IS_POSIX)

void SpinningMutex::LockSlow() {
  int retval = pthread_mutex_lock(&lock_);
  PA_DCHECK(retval == 0);
}

#elif BUILDFLAG(IS_FUCHSIA)

void SpinningMutex::LockSlow() {
  sync_mutex_lock(&lock_);
}

#endif

#else  // defined(PA_HAS_FAST_MUTEX)

void SpinningMutex::LockSlowSpinLock() {
  int yield_thread_count = 0;
  do {
    if (yield_thread_count < 10) {
      PA_YIELD_THREAD;
      yield_thread_count++;
    } else {
      // At this point, it's likely that the lock is held by a lower priority
      // thread that is unavailable to finish its work because of higher
      // priority threads spinning here. Sleeping should ensure that they make
      // progress.
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  } while (!TrySpinLock());
}

#endif  // defined(PA_HAS_FAST_MUTEX)

}  // namespace partition_alloc
