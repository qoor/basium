// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_DELAYED_TASK_MANAGER_H_
#define BASE_TASK_THREAD_POOL_DELAYED_TASK_MANAGER_H_

#include <functional>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/intrusive_heap.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/task.h"
#include "base/thread_annotations.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

class SequencedTaskRunner;

namespace internal {

// The DelayedTaskManager forwards tasks to post task callbacks when they become
// ripe for execution. Tasks are not forwarded before Start() is called. This
// class is thread-safe.
class BASE_EXPORT DelayedTaskManager {
 public:
  // Posts |task| for execution immediately.
  using PostTaskNowCallback = OnceCallback<void(Task task)>;

  // |tick_clock| can be specified for testing.
  DelayedTaskManager(
      const TickClock* tick_clock = DefaultTickClock::GetInstance());
  DelayedTaskManager(const DelayedTaskManager&) = delete;
  DelayedTaskManager& operator=(const DelayedTaskManager&) = delete;
  ~DelayedTaskManager();

  // Starts the delayed task manager, allowing past and future tasks to be
  // forwarded to their callbacks as they become ripe for execution.
  // |service_thread_task_runner| posts tasks to the ThreadPool service
  // thread.
  void Start(scoped_refptr<SequencedTaskRunner> service_thread_task_runner);

  // Schedules a call to |post_task_now_callback| with |task| as argument when
  // |task| is ripe for execution. |task_runner| is passed to retain a
  // reference until |task| is ripe.
  void AddDelayedTask(Task task,
                      PostTaskNowCallback post_task_now_callback,
                      scoped_refptr<TaskRunner> task_runner);

  // Pop and post all the ripe tasks in the delayed task queue.
  void ProcessRipeTasks();

  // Returns the |delayed_run_time| of the next scheduled task, if any.
  absl::optional<TimeTicks> NextScheduledRunTime() const;

 private:
  struct DelayedTask {
    DelayedTask();
    DelayedTask(Task task,
                PostTaskNowCallback callback,
                scoped_refptr<TaskRunner> task_runner);
    DelayedTask(DelayedTask&& other);
    DelayedTask(const DelayedTask&) = delete;
    DelayedTask& operator=(const DelayedTask&) = delete;
    ~DelayedTask();

    // Required by IntrusiveHeap::insert().
    DelayedTask& operator=(DelayedTask&& other);

    // Used for a min-heap.
    bool operator>(const DelayedTask& other) const;

    Task task;
    PostTaskNowCallback callback;
    scoped_refptr<TaskRunner> task_runner;

    // True iff the delayed task has been marked as scheduled.
    bool IsScheduled() const;

    // Mark the delayed task as scheduled. Since the sort key is
    // |task.delayed_run_time|, it does not alter sort order when it is called.
    void SetScheduled();

    // Required by IntrusiveHeap.
    void SetHeapHandle(const HeapHandle& handle) {}

    // Required by IntrusiveHeap.
    void ClearHeapHandle() {}

    // Required by IntrusiveHeap.
    HeapHandle GetHeapHandle() const { return HeapHandle::Invalid(); }

   private:
    bool scheduled_ = false;
  };

  // Get the time at which to schedule the next |ProcessRipeTasks()| execution,
  // or TimeTicks::Max() if none needs to be scheduled (i.e. no task, or next
  // task already scheduled).
  TimeTicks GetTimeToScheduleProcessRipeTasksLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(queue_lock_);

  // Schedule |ProcessRipeTasks()| on the service thread to be executed at the
  // given |process_ripe_tasks_time|, provided the given time is not
  // TimeTicks::Max().
  void ScheduleProcessRipeTasksOnServiceThread(
      TimeTicks process_ripe_tasks_time);

  const RepeatingClosure process_ripe_tasks_closure_;

  const TickClock* const tick_clock_;

  // Synchronizes access to |delayed_task_queue_| and the setting of
  // |service_thread_task_runner_|. Once |service_thread_task_runner_| is set,
  // it is never modified. It is therefore safe to access
  // |service_thread_task_runner_| without synchronization once it is observed
  // that it is non-null.
  mutable CheckedLock queue_lock_;

  scoped_refptr<SequencedTaskRunner> service_thread_task_runner_;

  IntrusiveHeap<DelayedTask, std::greater<>> delayed_task_queue_
      GUARDED_BY(queue_lock_);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_DELAYED_TASK_MANAGER_H_
