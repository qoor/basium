// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_pool_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/sequence_token.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"

namespace base {
namespace internal {

namespace {

constexpr char kPoolNameSuffix[] = "Pool";
constexpr char kDetachDurationHistogramPrefix[] =
    "TaskScheduler.DetachDuration.";
constexpr char kNumTasksBeforeDetachHistogramPrefix[] =
    "TaskScheduler.NumTasksBeforeDetach.";
constexpr char kNumTasksBetweenWaitsHistogramPrefix[] =
    "TaskScheduler.NumTasksBetweenWaits.";

// SchedulerWorkerPool that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const SchedulerWorkerPool>>::Leaky
    tls_current_worker_pool = LAZY_INSTANCE_INITIALIZER;

// A task runner that runs tasks with the PARALLEL ExecutionMode.
class SchedulerParallelTaskRunner : public TaskRunner {
 public:
  // Constructs a SchedulerParallelTaskRunner which can be used to post tasks so
  // long as |worker_pool| is alive.
  // TODO(robliao): Find a concrete way to manage |worker_pool|'s memory.
  SchedulerParallelTaskRunner(const TaskTraits& traits,
                              SchedulerWorkerPool* worker_pool)
      : traits_(traits), worker_pool_(worker_pool) {
    DCHECK(worker_pool_);
  }

  // TaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       OnceClosure closure,
                       TimeDelta delay) override {
    // Post the task as part of a one-off single-task Sequence.
    return worker_pool_->PostTaskWithSequence(
        MakeUnique<Task>(from_here, std::move(closure), traits_, delay),
        make_scoped_refptr(new Sequence));
  }

  bool RunsTasksInCurrentSequence() const override {
    return tls_current_worker_pool.Get().Get() == worker_pool_;
  }

 private:
  ~SchedulerParallelTaskRunner() override = default;

  const TaskTraits traits_;
  SchedulerWorkerPool* const worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerParallelTaskRunner);
};

// A task runner that runs tasks with the SEQUENCED ExecutionMode.
class SchedulerSequencedTaskRunner : public SequencedTaskRunner {
 public:
  // Constructs a SchedulerSequencedTaskRunner which can be used to post tasks
  // so long as |worker_pool| is alive.
  // TODO(robliao): Find a concrete way to manage |worker_pool|'s memory.
  SchedulerSequencedTaskRunner(const TaskTraits& traits,
                               SchedulerWorkerPool* worker_pool)
      : traits_(traits), worker_pool_(worker_pool) {
    DCHECK(worker_pool_);
  }

  // SequencedTaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       OnceClosure closure,
                       TimeDelta delay) override {
    std::unique_ptr<Task> task(
        new Task(from_here, std::move(closure), traits_, delay));
    task->sequenced_task_runner_ref = this;

    // Post the task as part of |sequence_|.
    return worker_pool_->PostTaskWithSequence(std::move(task), sequence_);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  OnceClosure closure,
                                  base::TimeDelta delay) override {
    // Tasks are never nested within the task scheduler.
    return PostDelayedTask(from_here, std::move(closure), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return sequence_->token() == SequenceToken::GetForCurrentThread();
  }

 private:
  ~SchedulerSequencedTaskRunner() override = default;

  // Sequence for all Tasks posted through this TaskRunner.
  const scoped_refptr<Sequence> sequence_ = new Sequence;

  const TaskTraits traits_;
  SchedulerWorkerPool* const worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequencedTaskRunner);
};

// Only used in DCHECKs.
bool ContainsWorker(const std::vector<scoped_refptr<SchedulerWorker>>& workers,
                    const SchedulerWorker* worker) {
  auto it = std::find_if(workers.begin(), workers.end(),
                         [worker](const scoped_refptr<SchedulerWorker>& i) {
                           return i.get() == worker;
                         });
  return it != workers.end();
}

}  // namespace

class SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl
    : public SchedulerWorker::Delegate {
 public:
  // |outer| owns the worker for which this delegate is constructed.
  SchedulerWorkerDelegateImpl(SchedulerWorkerPoolImpl* outer);
  ~SchedulerWorkerDelegateImpl() override;

  // SchedulerWorker::Delegate:
  void OnMainEntry(SchedulerWorker* worker) override;
  scoped_refptr<Sequence> GetWork(SchedulerWorker* worker) override;
  void DidRunTask() override;
  void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override;
  TimeDelta GetSleepTimeout() override;
  void OnMainExit(SchedulerWorker* worker) override;

 private:
  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // pool. Called from GetWork() when no work is available.
  bool CanCleanup(SchedulerWorker* worker);

  // Calls cleanup on |worker| and removes it from the pool.
  void Cleanup(SchedulerWorker* worker);

  SchedulerWorkerPoolImpl* outer_;

  // Time of the last detach.
  TimeTicks last_detach_time_;

  // Time when GetWork() first returned nullptr.
  TimeTicks idle_start_time_;

  // Number of tasks executed since the last time the
  // TaskScheduler.NumTasksBetweenWaits histogram was recorded.
  size_t num_tasks_since_last_wait_ = 0;

  // Number of tasks executed since the last time the
  // TaskScheduler.NumTasksBeforeDetach histogram was recorded.
  size_t num_tasks_since_last_detach_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerDelegateImpl);
};

SchedulerWorkerPoolImpl::SchedulerWorkerPoolImpl(
    const std::string& name,
    ThreadPriority priority_hint,
    TaskTracker* task_tracker,
    DelayedTaskManager* delayed_task_manager)
    : name_(name),
      priority_hint_(priority_hint),
      lock_(shared_priority_queue_.container_lock()),
      idle_workers_stack_cv_for_testing_(lock_.CreateConditionVariable()),
      join_for_testing_returned_(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED),
      // Mimics the UMA_HISTOGRAM_LONG_TIMES macro.
      detach_duration_histogram_(Histogram::FactoryTimeGet(
          kDetachDurationHistogramPrefix + name_ + kPoolNameSuffix,
          TimeDelta::FromMilliseconds(1),
          TimeDelta::FromHours(1),
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_1000 macro. When a worker runs more
      // than 1000 tasks before detaching, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_before_detach_histogram_(Histogram::FactoryGet(
          kNumTasksBeforeDetachHistogramPrefix + name_ + kPoolNameSuffix,
          1,
          1000,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_100 macro. A SchedulerWorker is
      // expected to run between zero and a few tens of tasks between waits.
      // When it runs more than 100 tasks, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_between_waits_histogram_(Histogram::FactoryGet(
          kNumTasksBetweenWaitsHistogramPrefix + name_ + kPoolNameSuffix,
          1,
          100,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      task_tracker_(task_tracker),
      delayed_task_manager_(delayed_task_manager) {
  DCHECK(task_tracker_);
  DCHECK(delayed_task_manager_);
}

void SchedulerWorkerPoolImpl::Start(const SchedulerWorkerPoolParams& params) {
  AutoSchedulerLock auto_lock(lock_);

  DCHECK(workers_.empty());

  worker_capacity_ = params.max_threads();
  suggested_reclaim_time_ = params.suggested_reclaim_time();
  backward_compatibility_ = params.backward_compatibility();

  // The initial number of workers is |num_wake_ups_before_start_| + 1 to try to
  // keep one at least one standby thread at all times (capacity permitting).
  const int num_initial_workers = std::min(num_wake_ups_before_start_ + 1,
                                           static_cast<int>(worker_capacity_));
  workers_.reserve(num_initial_workers);

  for (int index = 0; index < num_initial_workers; ++index) {
    SchedulerWorker* worker = CreateRegisterAndStartSchedulerWorker();

    // CHECK that the first worker can be started (assume that failure means
    // that threads can't be created on this machine).
    CHECK(worker || index > 0);

    if (worker) {
      if (index < num_wake_ups_before_start_)
        worker->WakeUp();
      else
        idle_workers_stack_.Push(worker);
    }
  }
}

SchedulerWorkerPoolImpl::~SchedulerWorkerPoolImpl() {
  // SchedulerWorkerPool should never be deleted in production unless its
  // initialization failed.
#if DCHECK_IS_ON()
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(join_for_testing_returned_.IsSignaled() || workers_.empty());
#endif
}

scoped_refptr<TaskRunner> SchedulerWorkerPoolImpl::CreateTaskRunnerWithTraits(
    const TaskTraits& traits) {
  return make_scoped_refptr(new SchedulerParallelTaskRunner(traits, this));
}

scoped_refptr<SequencedTaskRunner>
SchedulerWorkerPoolImpl::CreateSequencedTaskRunnerWithTraits(
    const TaskTraits& traits) {
  return make_scoped_refptr(new SchedulerSequencedTaskRunner(traits, this));
}

bool SchedulerWorkerPoolImpl::PostTaskWithSequence(
    std::unique_ptr<Task> task,
    scoped_refptr<Sequence> sequence) {
  DCHECK(task);
  DCHECK(sequence);

  if (!task_tracker_->WillPostTask(task.get()))
    return false;

  if (task->delayed_run_time.is_null()) {
    PostTaskWithSequenceNow(std::move(task), std::move(sequence));
  } else {
    // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
    // for details.
    CHECK(task->task);
    delayed_task_manager_->AddDelayedTask(
        std::move(task),
        BindOnce(
            [](scoped_refptr<Sequence> sequence,
               SchedulerWorkerPool* worker_pool, std::unique_ptr<Task> task) {
              worker_pool->PostTaskWithSequenceNow(std::move(task),
                                                   std::move(sequence));
            },
            std::move(sequence), Unretained(this)));
  }

  return true;
}

void SchedulerWorkerPoolImpl::PostTaskWithSequenceNow(
    std::unique_ptr<Task> task,
    scoped_refptr<Sequence> sequence) {
  DCHECK(task);
  DCHECK(sequence);

  // Confirm that |task| is ready to run (its delayed run time is either null or
  // in the past).
  DCHECK_LE(task->delayed_run_time, TimeTicks::Now());

  const bool sequence_was_empty = sequence->PushTask(std::move(task));
  if (sequence_was_empty) {
    // Insert |sequence| in |shared_priority_queue_| if it was empty before
    // |task| was inserted into it. Otherwise, one of these must be true:
    // - |sequence| is already in a PriorityQueue, or,
    // - A worker is running a Task from |sequence|. It will insert |sequence|
    //   in a PriorityQueue once it's done running the Task.
    const auto sequence_sort_key = sequence->GetSortKey();
    shared_priority_queue_.BeginTransaction()->Push(std::move(sequence),
                                                    sequence_sort_key);

    // Wake up a worker to process |sequence|.
    WakeUpOneWorker();
  }
}

void SchedulerWorkerPoolImpl::GetHistograms(
    std::vector<const HistogramBase*>* histograms) const {
  histograms->push_back(detach_duration_histogram_);
  histograms->push_back(num_tasks_between_waits_histogram_);
}

// TODO(jeffreyhe): Add and return an |initial_worker_capacity_| member when
// worker capacity becomes dynamic.
int SchedulerWorkerPoolImpl::GetMaxConcurrentTasksDeprecated() const {
#if DCHECK_IS_ON()
  AutoSchedulerLock auto_lock(lock_);
  DCHECK_NE(worker_capacity_, 0U) << "GetMaxConcurrentTasksDeprecated() should "
                                     "only be called after the worker pool has "
                                     "started.";
#endif
  return worker_capacity_;
}

void SchedulerWorkerPoolImpl::WaitForAllWorkersIdleForTesting() {
  AutoSchedulerLock auto_lock(lock_);
  while (idle_workers_stack_.Size() < workers_.size())
    idle_workers_stack_cv_for_testing_->Wait();
}

void SchedulerWorkerPoolImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  join_for_testing_started_.Set();
#endif
  DCHECK(!CanWorkerCleanupForTesting() || suggested_reclaim_time_.is_max())
      << "Workers can cleanup during join.";

  decltype(workers_) workers_copy;
  {
    AutoSchedulerLock auto_lock(lock_);

    // Make a copy of the SchedulerWorkers so that we can call
    // SchedulerWorker::JoinForTesting() without holding |lock_| since
    // SchedulerWorkers may need to access |workers_|.
    workers_copy = workers_;
  }
  for (const auto& worker : workers_copy)
    worker->JoinForTesting();

#if DCHECK_IS_ON()
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
#endif

  DCHECK(!join_for_testing_returned_.IsSignaled());
  join_for_testing_returned_.Signal();
}

void SchedulerWorkerPoolImpl::DisallowWorkerCleanupForTesting() {
  worker_cleanup_disallowed_.Set();
}

size_t SchedulerWorkerPoolImpl::NumberOfWorkersForTesting() {
  AutoSchedulerLock auto_lock(lock_);
  return workers_.size();
}

size_t SchedulerWorkerPoolImpl::GetWorkerCapacityForTesting() {
  AutoSchedulerLock auto_lock(lock_);
  return worker_capacity_;
}

SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    SchedulerWorkerDelegateImpl(SchedulerWorkerPoolImpl* outer)
    : outer_(outer) {}

SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    ~SchedulerWorkerDelegateImpl() = default;

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainEntry(
    SchedulerWorker* worker) {
  {
#if DCHECK_IS_ON()
    AutoSchedulerLock auto_lock(outer_->lock_);
    DCHECK(ContainsWorker(outer_->workers_, worker));
#endif
  }

  DCHECK_EQ(num_tasks_since_last_wait_, 0U);

  PlatformThread::SetName(
      StringPrintf("TaskScheduler%sWorker", outer_->name_.c_str()));

  DCHECK(!tls_current_worker_pool.Get().Get());
  tls_current_worker_pool.Get().Set(outer_);

  // New threads haven't run GetWork() yet, so reset the |idle_start_time_|.
  idle_start_time_ = TimeTicks();
}

scoped_refptr<Sequence>
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetWork(
    SchedulerWorker* worker) {
  {
#if DCHECK_IS_ON()
    AutoSchedulerLock auto_lock(outer_->lock_);
#endif
    DCHECK(ContainsWorker(outer_->workers_, worker));
  }

  scoped_refptr<Sequence> sequence;
  {
    std::unique_ptr<PriorityQueue::Transaction> shared_transaction(
        outer_->shared_priority_queue_.BeginTransaction());

    if (shared_transaction->IsEmpty()) {
      // |shared_transaction| is kept alive while |worker| is added to
      // |idle_workers_stack_| to avoid this race:
      // 1. This thread creates a Transaction, finds |shared_priority_queue_|
      //    empty and ends the Transaction.
      // 2. Other thread creates a Transaction, inserts a Sequence into
      //    |shared_priority_queue_| and ends the Transaction. This can't happen
      //    if the Transaction of step 1 is still active because because there
      //    can only be one active Transaction per PriorityQueue at a time.
      // 3. Other thread calls WakeUpOneWorker(). No thread is woken up because
      //    |idle_workers_stack_| is empty.
      // 4. This thread adds itself to |idle_workers_stack_| and goes to sleep.
      //    No thread runs the Sequence inserted in step 2.
      AutoSchedulerLock auto_lock(outer_->lock_);

      // Record the TaskScheduler.NumTasksBetweenWaits histogram. After
      // returning nullptr, the SchedulerWorker will perform a wait on its
      // WaitableEvent, so we record how many tasks were ran since the last wait
      // here.
      outer_->num_tasks_between_waits_histogram_->Add(
          num_tasks_since_last_wait_);
      num_tasks_since_last_wait_ = 0;

      if (CanCleanup(worker)) {
        Cleanup(worker);
        return nullptr;
      }

      outer_->AddToIdleWorkersStack(worker);
      if (idle_start_time_.is_null())
        idle_start_time_ = TimeTicks::Now();
      return nullptr;
    }

    sequence = shared_transaction->PopSequence();
  }
  DCHECK(sequence);
  {
    AutoSchedulerLock auto_lock(outer_->lock_);
    outer_->RemoveFromIdleWorkersStack(worker);
  }
  idle_start_time_ = TimeTicks();

  return sequence;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::DidRunTask() {
  ++num_tasks_since_last_wait_;
  ++num_tasks_since_last_detach_;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    ReEnqueueSequence(scoped_refptr<Sequence> sequence) {
  const SequenceSortKey sequence_sort_key = sequence->GetSortKey();
  outer_->shared_priority_queue_.BeginTransaction()->Push(std::move(sequence),
                                                          sequence_sort_key);
  // The thread calling this method will soon call GetWork(). Therefore, there
  // is no need to wake up a worker to run the sequence that was just inserted
  // into |outer_->shared_priority_queue_|.
}

TimeDelta SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    GetSleepTimeout() {
  return outer_->suggested_reclaim_time_;
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::CanCleanup(
    SchedulerWorker* worker) {
  const bool can_cleanup =
      !idle_start_time_.is_null() &&
      (TimeTicks::Now() - idle_start_time_) > outer_->suggested_reclaim_time_ &&
      worker != outer_->PeekAtIdleWorkersStack() &&
      outer_->CanWorkerCleanupForTesting();
  return can_cleanup;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::Cleanup(
    SchedulerWorker* worker) {
  outer_->num_tasks_before_detach_histogram_->Add(num_tasks_since_last_detach_);
  outer_->cleanup_timestamps_.push(TimeTicks::Now());
  worker->Cleanup();
  outer_->RemoveFromIdleWorkersStack(worker);

  // Remove the worker from |workers_|.
  auto worker_iter =
      std::find(outer_->workers_.begin(), outer_->workers_.end(), worker);
  DCHECK(worker_iter != outer_->workers_.end());
  outer_->workers_.erase(worker_iter);
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainExit(
    SchedulerWorker* worker) {
#if DCHECK_IS_ON()
  bool shutdown_complete = outer_->task_tracker_->IsShutdownComplete();
  AutoSchedulerLock auto_lock(outer_->lock_);

  // |worker| should already have been removed from the idle workers stack and
  // |workers_| by the time the thread is about to exit. (except in the cases
  // where the pool is no longer going to be used - in which case, it's fine for
  // there to be invalid workers in the pool.
  if (!shutdown_complete && !outer_->join_for_testing_started_.IsSet()) {
    DCHECK(!outer_->idle_workers_stack_.Contains(worker));
    DCHECK(!ContainsWorker(outer_->workers_, worker));
  }
#endif
}

void SchedulerWorkerPoolImpl::WakeUpOneWorker() {
  SchedulerWorker* worker = nullptr;

  AutoSchedulerLock auto_lock(lock_);

  if (workers_.empty()) {
    ++num_wake_ups_before_start_;
    return;
  }

  // Add a new worker if we're below capacity and there are no idle workers.
  if (idle_workers_stack_.IsEmpty() && workers_.size() < worker_capacity_)
    worker = CreateRegisterAndStartSchedulerWorker();
  else
    worker = idle_workers_stack_.Pop();

  if (worker)
    worker->WakeUp();

  // Try to keep at least one idle worker at all times for better
  // responsiveness.
  if (idle_workers_stack_.IsEmpty() && workers_.size() < worker_capacity_) {
    SchedulerWorker* new_worker = CreateRegisterAndStartSchedulerWorker();
    if (new_worker)
      idle_workers_stack_.Push(new_worker);
  }
}

void SchedulerWorkerPoolImpl::AddToIdleWorkersStack(
    SchedulerWorker* worker) {
  lock_.AssertAcquired();

  // Waking up after the sleep timeout may cause multiple attempts to add to the
  // idle stack. After waking up from the sleep timeout, the worker will be on
  // the idle stack. If the worker then calls GetWork() and finds no work, but
  // CanCleanup() returns false, then the worker will get added to the idle
  // stack while already being on it.
  if (!idle_workers_stack_.Contains(worker))
    idle_workers_stack_.Push(worker);

  DCHECK_LE(idle_workers_stack_.Size(), workers_.size());

  if (idle_workers_stack_.Size() == workers_.size())
    idle_workers_stack_cv_for_testing_->Broadcast();
}

const SchedulerWorker* SchedulerWorkerPoolImpl::PeekAtIdleWorkersStack() const {
  lock_.AssertAcquired();
  return idle_workers_stack_.Peek();
}

void SchedulerWorkerPoolImpl::RemoveFromIdleWorkersStack(
    SchedulerWorker* worker) {
  lock_.AssertAcquired();
  idle_workers_stack_.Remove(worker);
}

bool SchedulerWorkerPoolImpl::CanWorkerCleanupForTesting() {
  return !worker_cleanup_disallowed_.IsSet();
}

SchedulerWorker*
SchedulerWorkerPoolImpl::CreateRegisterAndStartSchedulerWorker() {
  lock_.AssertAcquired();

  DCHECK_LT(workers_.size(), worker_capacity_);

  // SchedulerWorker needs |lock_| as a predecessor for its thread lock
  // because in WakeUpOneWorker, |lock_| is first acquired and then
  // the thread lock is acquired when WakeUp is called on the worker.
  scoped_refptr<SchedulerWorker> worker = MakeRefCounted<SchedulerWorker>(
      priority_hint_, MakeUnique<SchedulerWorkerDelegateImpl>(this),
      task_tracker_, &lock_, backward_compatibility_);

  if (!worker->Start())
    return nullptr;

  workers_.push_back(worker);

  if (!cleanup_timestamps_.empty()) {
    detach_duration_histogram_->AddTime(TimeTicks::Now() -
                                        cleanup_timestamps_.top());
    cleanup_timestamps_.pop();
  }
  return worker.get();
}

}  // namespace internal
}  // namespace base
