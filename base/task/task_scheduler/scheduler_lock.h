// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_SCHEDULER_LOCK_H_
#define BASE_TASK_TASK_SCHEDULER_SCHEDULER_LOCK_H_

#include <memory>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/task_scheduler/scheduler_lock_impl.h"
#include "base/thread_annotations.h"

namespace base {
namespace internal {

// SchedulerLock should be used anywhere a lock would be used in the scheduler.
// When DCHECK_IS_ON(), lock checking occurs. Otherwise, SchedulerLock is
// equivalent to base::Lock.
//
// The shape of SchedulerLock is as follows:
// SchedulerLock()
//     Default constructor, no predecessor lock.
//     DCHECKs
//         On Acquisition if any scheduler lock is acquired on this thread.
//             Okay if a universal predecessor is acquired.
//
// SchedulerLock(const SchedulerLock* predecessor)
//     Constructor that specifies an allowed predecessor for that lock.
//     DCHECKs
//         On Construction if |predecessor| forms a predecessor lock cycle.
//         On Acquisition if the previous lock acquired on the thread is not
//             either |predecessor| or a universal predecessor. Okay if there
//             was no previous lock acquired.
//
// SchedulerLock(UniversalPredecessor universal_predecessor)
//     Constructor for a lock that will allow the acquisition of any lock after
//     it, without needing to explicitly be named a predecessor. Can only be
//     acquired if no locks are currently held by this thread.
//     DCHECKs
//         On Acquisition if any scheduler lock is acquired on this thread.
//
// void Acquire()
//     Acquires the lock.
//
// void Release()
//     Releases the lock.
//
// void AssertAcquired().
//     DCHECKs if the lock is not acquired.
//
// std::unique_ptr<ConditionVariable> CreateConditionVariable()
//     Creates a condition variable using this as a lock.

#if DCHECK_IS_ON()
class LOCKABLE SchedulerLock : public SchedulerLockImpl {
 public:
  SchedulerLock() = default;
  explicit SchedulerLock(const SchedulerLock* predecessor)
      : SchedulerLockImpl(predecessor) {}
  explicit SchedulerLock(UniversalPredecessor universal_predecessor)
      : SchedulerLockImpl(universal_predecessor) {}
};
#else   // DCHECK_IS_ON()
class LOCKABLE SchedulerLock : public Lock {
 public:
  SchedulerLock() = default;
  explicit SchedulerLock(const SchedulerLock*) {}
  explicit SchedulerLock(UniversalPredecessor) {}
  static void AssertNoLockHeldOnCurrentThread() {}

  std::unique_ptr<ConditionVariable> CreateConditionVariable() {
    return std::unique_ptr<ConditionVariable>(new ConditionVariable(this));
  }
};
#endif  // DCHECK_IS_ON()

// Provides the same functionality as base::AutoLock for SchedulerLock.
class SCOPED_LOCKABLE AutoSchedulerLock {
 public:
  explicit AutoSchedulerLock(SchedulerLock& lock) EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoSchedulerLock() UNLOCK_FUNCTION() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  SchedulerLock& lock_;

  DISALLOW_COPY_AND_ASSIGN(AutoSchedulerLock);
};

// Informs the clang thread safety analysis that an aliased lock is acquired.
// Because the clang thread safety analysis doesn't understand aliased locks
// [1], this code wouldn't compile without AnnotateAcquiredLockAlias:
//
// class Example {
//  public:
//    SchedulerLock lock_;
//    int value = 0 GUARDED_BY(lock_);
// };
//
// Example example;
// SchedulerLock* acquired = &example.lock_;
// AutoSchedulerLock auto_lock(*acquired);
// AnnotateAcquiredLockAlias annotate(*acquired, example.lock_);
// example.value = 42;  // Doesn't compile without |annotate|.
//
// [1] https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#no-alias-analysis
class SCOPED_LOCKABLE AnnotateAcquiredLockAlias {
 public:
  // |acquired_lock| is an acquired lock. |lock_alias| is an alias of
  // |acquired_lock|.
  AnnotateAcquiredLockAlias(const SchedulerLock& acquired_lock,
                            const SchedulerLock& lock_alias)
      EXCLUSIVE_LOCK_FUNCTION(lock_alias)
      : acquired_lock_(acquired_lock) {
    DCHECK_EQ(&acquired_lock, &lock_alias);
    acquired_lock_.AssertAcquired();
  }
  ~AnnotateAcquiredLockAlias() UNLOCK_FUNCTION() {
    acquired_lock_.AssertAcquired();
  }

 private:
  const SchedulerLock& acquired_lock_;

  DISALLOW_COPY_AND_ASSIGN(AnnotateAcquiredLockAlias);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_SCHEDULER_LOCK_H_
