// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/profiler/native_stack_sampler.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"

namespace base {

const size_t kUnknownModuleIndex = static_cast<size_t>(-1);

namespace {

// This value is used to initialize the WaitableEvent object. This MUST BE set
// to MANUAL for correct operation of the IsSignaled() call in Start(). See the
// comment there for why.
constexpr WaitableEvent::ResetPolicy kResetPolicy =
    WaitableEvent::ResetPolicy::MANUAL;

// This value is used when there is no collection in progress and thus no ID
// for referencing the active collection to the SamplingThread.
const int kNullProfilerId = -1;

void ChangeAtomicFlags(subtle::Atomic32* flags,
                       subtle::Atomic32 set,
                       subtle::Atomic32 clear) {
  DCHECK(set != 0 || clear != 0);
  DCHECK_EQ(0, set & clear);

  subtle::Atomic32 bits = subtle::NoBarrier_Load(flags);
  while (true) {
    subtle::Atomic32 existing =
        subtle::NoBarrier_CompareAndSwap(flags, bits, (bits | set) & ~clear);
    if (existing == bits)
      break;
    bits = existing;
  }
}

}  // namespace

// StackSamplingProfiler::Module ----------------------------------------------

StackSamplingProfiler::Module::Module() : base_address(0u) {}

StackSamplingProfiler::Module::Module(uintptr_t base_address,
                                      const std::string& id,
                                      const FilePath& filename)
    : base_address(base_address), id(id), filename(filename) {}

StackSamplingProfiler::Module::~Module() = default;

// StackSamplingProfiler::InternalModule --------------------------------------

StackSamplingProfiler::InternalModule::InternalModule() : is_valid(false) {}

StackSamplingProfiler::InternalModule::InternalModule(uintptr_t base_address,
                                                      const std::string& id,
                                                      const FilePath& filename)
    : base_address(base_address), id(id), filename(filename), is_valid(true) {}

StackSamplingProfiler::InternalModule::~InternalModule() = default;

// StackSamplingProfiler::Frame -----------------------------------------------

StackSamplingProfiler::Frame::Frame(uintptr_t instruction_pointer,
                                    size_t module_index)
    : instruction_pointer(instruction_pointer), module_index(module_index) {}

StackSamplingProfiler::Frame::~Frame() = default;

StackSamplingProfiler::Frame::Frame()
    : instruction_pointer(0), module_index(kUnknownModuleIndex) {}

// StackSamplingProfiler::InternalFrame -------------------------------------

StackSamplingProfiler::InternalFrame::InternalFrame(
    uintptr_t instruction_pointer,
    InternalModule internal_module)
    : instruction_pointer(instruction_pointer),
      internal_module(std::move(internal_module)) {}

StackSamplingProfiler::InternalFrame::~InternalFrame() = default;

// StackSamplingProfiler::Sample ----------------------------------------------

StackSamplingProfiler::Sample::Sample() = default;

StackSamplingProfiler::Sample::Sample(const Sample& sample) = default;

StackSamplingProfiler::Sample::~Sample() = default;

StackSamplingProfiler::Sample::Sample(const Frame& frame) {
  frames.push_back(std::move(frame));
}

StackSamplingProfiler::Sample::Sample(const std::vector<Frame>& frames)
    : frames(frames) {}

// StackSamplingProfiler::CallStackProfile ------------------------------------

StackSamplingProfiler::CallStackProfile::CallStackProfile() = default;

StackSamplingProfiler::CallStackProfile::CallStackProfile(
    CallStackProfile&& other) = default;

StackSamplingProfiler::CallStackProfile::~CallStackProfile() = default;

StackSamplingProfiler::CallStackProfile&
StackSamplingProfiler::CallStackProfile::operator=(CallStackProfile&& other) =
    default;

StackSamplingProfiler::CallStackProfile
StackSamplingProfiler::CallStackProfile::CopyForTesting() const {
  return CallStackProfile(*this);
}

StackSamplingProfiler::CallStackProfile::CallStackProfile(
    const CallStackProfile& other) = default;

// StackSamplingProfiler::SamplingThread --------------------------------------

class StackSamplingProfiler::SamplingThread : public Thread {
 public:
  class TestAPI {
   public:
    // Reset the existing sampler. This will unfortunately create the object
    // unnecessarily if it doesn't already exist but there's no way around that.
    static void Reset();

    // Disables inherent idle-shutdown behavior.
    static void DisableIdleShutdown();

    // Begins an idle shutdown as if the idle-timer had expired and wait for
    // it to execute. Since the timer would have only been started at a time
    // when the sampling thread actually was idle, this must be called only
    // when it is known that there are no active sampling threads. If
    // |simulate_intervening_add| is true then, when executed, the shutdown
    // task will believe that a new collection has been added since it was
    // posted.
    static void ShutdownAssumingIdle(bool simulate_intervening_add);

   private:
    // Calls the sampling threads ShutdownTask and then signals an event.
    static void ShutdownTaskAndSignalEvent(SamplingThread* sampler,
                                           int add_events,
                                           WaitableEvent* event);
  };

  struct CollectionContext {
    CollectionContext(PlatformThreadId target,
                      const SamplingParams& params,
                      WaitableEvent* finished,
                      std::unique_ptr<NativeStackSampler> sampler,
                      std::unique_ptr<ProfileBuilder> profile_builder)
        : collection_id(next_collection_id.GetNext()),
          target(target),
          params(params),
          finished(finished),
          native_sampler(std::move(sampler)),
          profile_builder(std::move(profile_builder)) {}
    ~CollectionContext() = default;

    // An identifier for this collection, used to uniquely identify the
    // collection to outside interests.
    const int collection_id;

    const PlatformThreadId target;  // ID of The thread being sampled.
    const SamplingParams params;    // Information about how to sample.
    WaitableEvent* const finished;  // Signaled when all sampling complete.

    // Platform-specific module that does the actual sampling.
    std::unique_ptr<NativeStackSampler> native_sampler;

    // Receives the sampling data and builds a CallStackProfile.
    std::unique_ptr<ProfileBuilder> profile_builder;

    // The absolute time for the next sample.
    Time next_sample_time;

    // The time that a profile was started, for calculating the total duration.
    Time profile_start_time;

    // Counter that indicates the current sample position along the acquisition.
    int sample_count = 0;

    // Sequence number for generating new collection ids.
    static AtomicSequenceNumber next_collection_id;
  };

  // Gets the single instance of this class.
  static SamplingThread* GetInstance();

  // Adds a new CollectionContext to the thread. This can be called externally
  // from any thread. This returns a collection id that can later be used to
  // stop the sampling.
  int Add(std::unique_ptr<CollectionContext> collection);

  // Removes an active collection based on its collection id, forcing it to run
  // its callback if any data has been collected. This can be called externally
  // from any thread.
  void Remove(int collection_id);

 private:
  friend class TestAPI;
  friend struct DefaultSingletonTraits<SamplingThread>;

  // The different states in which the sampling-thread can be.
  enum ThreadExecutionState {
    // The thread is not running because it has never been started. It will be
    // started when a sampling request is received.
    NOT_STARTED,

    // The thread is running and processing tasks. This is the state when any
    // sampling requests are active and during the "idle" period afterward
    // before the thread is stopped.
    RUNNING,

    // Once all sampling requests have finished and the "idle" period has
    // expired, the thread will be set to this state and its shutdown
    // initiated. A call to Stop() must be made to ensure the previous thread
    // has completely exited before calling Start() and moving back to the
    // RUNNING state.
    EXITING,
  };

  SamplingThread();
  ~SamplingThread() override;

  // Get task runner that is usable from the outside.
  scoped_refptr<SingleThreadTaskRunner> GetOrCreateTaskRunnerForAdd();
  scoped_refptr<SingleThreadTaskRunner> GetTaskRunner(
      ThreadExecutionState* out_state);

  // Get task runner that is usable from the sampling thread itself.
  scoped_refptr<SingleThreadTaskRunner> GetTaskRunnerOnSamplingThread();

  // Finishes a collection. The collection's |finished| waitable event will be
  // signalled. The |collection| should already have been removed from
  // |active_collections_| by the caller, as this is needed to avoid flakiness
  // in unit tests.
  void FinishCollection(CollectionContext* collection);

  // Check if the sampling thread is idle and begin a shutdown if it is.
  void ScheduleShutdownIfIdle();

  // These methods are tasks that get posted to the internal message queue.
  void AddCollectionTask(std::unique_ptr<CollectionContext> collection);
  void RemoveCollectionTask(int collection_id);
  void RecordSampleTask(int collection_id);
  void ShutdownTask(int add_events);

  // Thread:
  void CleanUp() override;

  // A stack-buffer used by the native sampler for its work. This buffer can
  // be re-used for multiple native sampler objects so long as the API calls
  // that take it are not called concurrently.
  std::unique_ptr<NativeStackSampler::StackBuffer> stack_buffer_;

  // A map of collection ids to collection contexts. Because this class is a
  // singleton that is never destroyed, context objects will never be destructed
  // except by explicit action. Thus, it's acceptable to pass unretained
  // pointers to these objects when posting tasks.
  std::map<int, std::unique_ptr<CollectionContext>> active_collections_;

  // State maintained about the current execution (or non-execution) of
  // the thread. This state must always be accessed while holding the
  // lock. A copy of the task-runner is maintained here for use by any
  // calling thread; this is necessary because Thread's accessor for it is
  // not itself thread-safe. The lock is also used to order calls to the
  // Thread API (Start, Stop, StopSoon, & DetachFromSequence) so that
  // multiple threads may make those calls.
  Lock thread_execution_state_lock_;  // Protects all thread_execution_state_*
  ThreadExecutionState thread_execution_state_ = NOT_STARTED;
  scoped_refptr<SingleThreadTaskRunner> thread_execution_state_task_runner_;
  bool thread_execution_state_disable_idle_shutdown_for_testing_ = false;

  // A counter that notes adds of new collection requests. It is incremented
  // when changes occur so that delayed shutdown tasks are able to detect if
  // something new has happened while it was waiting. Like all "execution_state"
  // vars, this must be accessed while holding |thread_execution_state_lock_|.
  int thread_execution_state_add_events_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SamplingThread);
};

// static
void StackSamplingProfiler::SamplingThread::TestAPI::Reset() {
  SamplingThread* sampler = SamplingThread::GetInstance();

  ThreadExecutionState state;
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    state = sampler->thread_execution_state_;
    DCHECK(sampler->active_collections_.empty());
  }

  // Stop the thread and wait for it to exit. This has to be done through by
  // the thread itself because it has taken ownership of its own lifetime.
  if (state == RUNNING) {
    ShutdownAssumingIdle(false);
    state = EXITING;
  }
  // Make sure thread is cleaned up since state will be reset to NOT_STARTED.
  if (state == EXITING)
    sampler->Stop();

  // Reset internal variables to the just-initialized state.
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    sampler->thread_execution_state_ = NOT_STARTED;
    sampler->thread_execution_state_task_runner_ = nullptr;
    sampler->thread_execution_state_disable_idle_shutdown_for_testing_ = false;
    sampler->thread_execution_state_add_events_ = 0;
  }
}

// static
void StackSamplingProfiler::SamplingThread::TestAPI::DisableIdleShutdown() {
  SamplingThread* sampler = SamplingThread::GetInstance();

  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    sampler->thread_execution_state_disable_idle_shutdown_for_testing_ = true;
  }
}

// static
void StackSamplingProfiler::SamplingThread::TestAPI::ShutdownAssumingIdle(
    bool simulate_intervening_add) {
  SamplingThread* sampler = SamplingThread::GetInstance();

  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      sampler->GetTaskRunner(&state);
  DCHECK_EQ(RUNNING, state);
  DCHECK(task_runner);

  int add_events;
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    add_events = sampler->thread_execution_state_add_events_;
    if (simulate_intervening_add)
      ++sampler->thread_execution_state_add_events_;
  }

  WaitableEvent executed(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  // PostTaskAndReply won't work because thread and associated message-loop may
  // be shut down.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&ShutdownTaskAndSignalEvent, Unretained(sampler),
                          add_events, Unretained(&executed)));
  executed.Wait();
}

// static
void StackSamplingProfiler::SamplingThread::TestAPI::ShutdownTaskAndSignalEvent(
    SamplingThread* sampler,
    int add_events,
    WaitableEvent* event) {
  sampler->ShutdownTask(add_events);
  event->Signal();
}

AtomicSequenceNumber StackSamplingProfiler::SamplingThread::CollectionContext::
    next_collection_id;

StackSamplingProfiler::SamplingThread::SamplingThread()
    : Thread("StackSamplingProfiler") {}

StackSamplingProfiler::SamplingThread::~SamplingThread() = default;

StackSamplingProfiler::SamplingThread*
StackSamplingProfiler::SamplingThread::GetInstance() {
  return Singleton<SamplingThread, LeakySingletonTraits<SamplingThread>>::get();
}

int StackSamplingProfiler::SamplingThread::Add(
    std::unique_ptr<CollectionContext> collection) {
  // This is not to be run on the sampling thread.

  int collection_id = collection->collection_id;
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      GetOrCreateTaskRunnerForAdd();

  task_runner->PostTask(
      FROM_HERE, BindOnce(&SamplingThread::AddCollectionTask, Unretained(this),
                          std::move(collection)));

  return collection_id;
}

void StackSamplingProfiler::SamplingThread::Remove(int collection_id) {
  // This is not to be run on the sampling thread.

  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner = GetTaskRunner(&state);
  if (state != RUNNING)
    return;
  DCHECK(task_runner);

  // This can fail if the thread were to exit between acquisition of the task
  // runner above and the call below. In that case, however, everything has
  // stopped so there's no need to try to stop it.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&SamplingThread::RemoveCollectionTask,
                                 Unretained(this), collection_id));
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetOrCreateTaskRunnerForAdd() {
  AutoLock lock(thread_execution_state_lock_);

  // The increment of the "add events" count is why this method is to be only
  // called from "add".
  ++thread_execution_state_add_events_;

  if (thread_execution_state_ == RUNNING) {
    DCHECK(thread_execution_state_task_runner_);
    // This shouldn't be called from the sampling thread as it's inefficient.
    // Use GetTaskRunnerOnSamplingThread() instead.
    DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());
    return thread_execution_state_task_runner_;
  }

  if (thread_execution_state_ == EXITING) {
    // StopSoon() was previously called to shut down the thread
    // asynchonously. Stop() must now be called before calling Start() again to
    // reset the thread state.
    //
    // We must allow blocking here to satisfy the Thread implementation, but in
    // practice the Stop() call is unlikely to actually block. For this to
    // happen a new profiling request would have to be made within the narrow
    // window between StopSoon() and thread exit following the end of the 60
    // second idle period.
    ScopedAllowBlocking allow_blocking;
    Stop();
  }

  DCHECK(!stack_buffer_);
  stack_buffer_ = NativeStackSampler::CreateStackBuffer();

  // The thread is not running. Start it and get associated runner. The task-
  // runner has to be saved for future use because though it can be used from
  // any thread, it can be acquired via task_runner() only on the created
  // thread and the thread that creates it (i.e. this thread) for thread-safety
  // reasons which are alleviated in SamplingThread by gating access to it with
  // the |thread_execution_state_lock_|.
  Start();
  thread_execution_state_ = RUNNING;
  thread_execution_state_task_runner_ = Thread::task_runner();

  // Detach the sampling thread from the "sequence" (i.e. thread) that
  // started it so that it can be self-managed or stopped by another thread.
  DetachFromSequence();

  return thread_execution_state_task_runner_;
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetTaskRunner(
    ThreadExecutionState* out_state) {
  AutoLock lock(thread_execution_state_lock_);
  if (out_state)
    *out_state = thread_execution_state_;
  if (thread_execution_state_ == RUNNING) {
    // This shouldn't be called from the sampling thread as it's inefficient.
    // Use GetTaskRunnerOnSamplingThread() instead.
    DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());
    DCHECK(thread_execution_state_task_runner_);
  } else {
    DCHECK(!thread_execution_state_task_runner_);
  }

  return thread_execution_state_task_runner_;
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetTaskRunnerOnSamplingThread() {
  // This should be called only from the sampling thread as it has limited
  // accessibility.
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  return Thread::task_runner();
}

void StackSamplingProfiler::SamplingThread::FinishCollection(
    CollectionContext* collection) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());
  DCHECK_EQ(0u, active_collections_.count(collection->collection_id));

  TimeDelta profile_duration = Time::Now() - collection->profile_start_time +
                               collection->params.sampling_interval;

  collection->profile_builder->OnProfileCompleted(
      profile_duration, collection->params.sampling_interval);

  // Signal that this collection is finished.
  collection->finished->Signal();

  ScheduleShutdownIfIdle();
}

void StackSamplingProfiler::SamplingThread::ScheduleShutdownIfIdle() {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  if (!active_collections_.empty())
    return;

  int add_events;
  {
    AutoLock lock(thread_execution_state_lock_);
    if (thread_execution_state_disable_idle_shutdown_for_testing_)
      return;
    add_events = thread_execution_state_add_events_;
  }

  GetTaskRunnerOnSamplingThread()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SamplingThread::ShutdownTask, Unretained(this), add_events),
      TimeDelta::FromSeconds(60));
}

void StackSamplingProfiler::SamplingThread::AddCollectionTask(
    std::unique_ptr<CollectionContext> collection) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  const int collection_id = collection->collection_id;
  const TimeDelta initial_delay = collection->params.initial_delay;

  active_collections_.insert(
      std::make_pair(collection_id, std::move(collection)));

  GetTaskRunnerOnSamplingThread()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SamplingThread::RecordSampleTask, Unretained(this),
               collection_id),
      initial_delay);

  // Another increment of "add events" serves to invalidate any pending
  // shutdown tasks that may have been initiated between the Add() and this
  // task running.
  {
    AutoLock lock(thread_execution_state_lock_);
    ++thread_execution_state_add_events_;
  }
}

void StackSamplingProfiler::SamplingThread::RemoveCollectionTask(
    int collection_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto found = active_collections_.find(collection_id);
  if (found == active_collections_.end())
    return;

  // Remove |collection| from |active_collections_|.
  std::unique_ptr<CollectionContext> collection = std::move(found->second);
  size_t count = active_collections_.erase(collection_id);
  DCHECK_EQ(1U, count);

  FinishCollection(collection.get());
}

void StackSamplingProfiler::SamplingThread::RecordSampleTask(
    int collection_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto found = active_collections_.find(collection_id);

  // The task won't be found if it has been stopped.
  if (found == active_collections_.end())
    return;

  CollectionContext* collection = found->second.get();

  // If this is the first sample, the collection params need to be filled.
  if (collection->sample_count == 0) {
    collection->profile_start_time = Time::Now();
    collection->next_sample_time = Time::Now();
    collection->native_sampler->ProfileRecordingStarting();
  }

  // Record a single sample.
  collection->profile_builder->OnSampleCompleted(
      collection->native_sampler->RecordStackFrames(
          stack_buffer_.get(), collection->profile_builder.get()));

  // Schedule the next sample recording if there is one.
  if (++collection->sample_count < collection->params.samples_per_profile) {
    // This will keep a consistent average interval between samples but will
    // result in constant series of acquisitions, thus nearly locking out the
    // target thread, if the interval is smaller than the time it takes to
    // actually acquire the sample. Anything sampling that quickly is going
    // to be a problem anyway so don't worry about it.
    collection->next_sample_time += collection->params.sampling_interval;
    bool success = GetTaskRunnerOnSamplingThread()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&SamplingThread::RecordSampleTask, Unretained(this),
                 collection_id),
        std::max(collection->next_sample_time - Time::Now(), TimeDelta()));
    DCHECK(success);
    return;
  }

  // Take ownership of |collection| and remove it from the map.
  std::unique_ptr<CollectionContext> owned_collection =
      std::move(found->second);
  size_t count = active_collections_.erase(collection_id);
  DCHECK_EQ(1U, count);

  // All capturing has completed so finish the collection.
  FinishCollection(collection);
}

void StackSamplingProfiler::SamplingThread::ShutdownTask(int add_events) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  // Holding this lock ensures that any attempt to start another job will
  // get postponed until |thread_execution_state_| is updated, thus eliminating
  // the race in starting a new thread while the previous one is exiting.
  AutoLock lock(thread_execution_state_lock_);

  // If the current count of creation requests doesn't match the passed count
  // then other tasks have been created since this was posted. Abort shutdown.
  if (thread_execution_state_add_events_ != add_events)
    return;

  // There can be no new AddCollectionTasks at this point because creating
  // those always increments "add events". There may be other requests, like
  // Remove, but it's okay to schedule the thread to stop once they've been
  // executed (i.e. "soon").
  DCHECK(active_collections_.empty());
  StopSoon();

  // StopSoon will have set the owning sequence (again) so it must be detached
  // (again) in order for Stop/Start to be called (again) should more work
  // come in. Holding the |thread_execution_state_lock_| ensures the necessary
  // happens-after with regard to this detach and future Thread API calls.
  DetachFromSequence();

  // Set the thread_state variable so the thread will be restarted when new
  // work comes in. Remove the |thread_execution_state_task_runner_| to avoid
  // confusion.
  thread_execution_state_ = EXITING;
  thread_execution_state_task_runner_ = nullptr;
  stack_buffer_.reset();
}

void StackSamplingProfiler::SamplingThread::CleanUp() {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  // There should be no collections remaining when the thread stops.
  DCHECK(active_collections_.empty());

  // Let the parent clean up.
  Thread::CleanUp();
}

// StackSamplingProfiler ------------------------------------------------------

// static
void StackSamplingProfiler::TestAPI::Reset() {
  SamplingThread::TestAPI::Reset();
  ResetAnnotations();
}

// static
void StackSamplingProfiler::TestAPI::ResetAnnotations() {
  subtle::NoBarrier_Store(&process_milestones_, 0u);
}

// static
bool StackSamplingProfiler::TestAPI::IsSamplingThreadRunning() {
  return SamplingThread::GetInstance()->IsRunning();
}

// static
void StackSamplingProfiler::TestAPI::DisableIdleShutdown() {
  SamplingThread::TestAPI::DisableIdleShutdown();
}

// static
void StackSamplingProfiler::TestAPI::PerformSamplingThreadIdleShutdown(
    bool simulate_intervening_start) {
  SamplingThread::TestAPI::ShutdownAssumingIdle(simulate_intervening_start);
}

subtle::Atomic32 StackSamplingProfiler::process_milestones_ = 0;

StackSamplingProfiler::StackSamplingProfiler(
    const SamplingParams& params,
    std::unique_ptr<ProfileBuilder> profile_builder,
    NativeStackSamplerTestDelegate* test_delegate)
    : StackSamplingProfiler(PlatformThread::CurrentId(),
                            params,
                            std::move(profile_builder),
                            test_delegate) {}

StackSamplingProfiler::StackSamplingProfiler(
    PlatformThreadId thread_id,
    const SamplingParams& params,
    std::unique_ptr<ProfileBuilder> profile_builder,
    NativeStackSamplerTestDelegate* test_delegate)
    : thread_id_(thread_id),
      params_(params),
      profile_builder_(std::move(profile_builder)),
      // The event starts "signaled" so code knows it's safe to start thread
      // and "manual" so that it can be waited in multiple places.
      profiling_inactive_(kResetPolicy, WaitableEvent::InitialState::SIGNALED),
      profiler_id_(kNullProfilerId),
      test_delegate_(test_delegate) {
  DCHECK(profile_builder_);
}

StackSamplingProfiler::~StackSamplingProfiler() {
  // Stop returns immediately but the shutdown runs asynchronously. There is a
  // non-zero probability that one more sample will be taken after this call
  // returns.
  Stop();

  // The behavior of sampling a thread that has exited is undefined and could
  // cause Bad Things(tm) to occur. The safety model provided by this class is
  // that an instance of this object is expected to live at least as long as
  // the thread it is sampling. However, because the sampling is performed
  // asynchronously by the SamplingThread, there is no way to guarantee this
  // is true without waiting for it to signal that it has finished.
  //
  // The wait time should, at most, be only as long as it takes to collect one
  // sample (~200us) or none at all if sampling has already completed.
  ThreadRestrictions::ScopedAllowWait allow_wait;
  profiling_inactive_.Wait();
}

void StackSamplingProfiler::Start() {
  // Multiple calls to Start() for a single StackSamplingProfiler object is not
  // allowed. If profile_builder_ is nullptr, then Start() has been called
  // already.
  DCHECK(profile_builder_);

  std::unique_ptr<NativeStackSampler> native_sampler =
      NativeStackSampler::Create(thread_id_, test_delegate_);

  if (!native_sampler)
    return;

  // The IsSignaled() check below requires that the WaitableEvent be manually
  // reset, to avoid signaling the event in IsSignaled() itself.
  static_assert(kResetPolicy == WaitableEvent::ResetPolicy::MANUAL,
                "The reset policy must be set to MANUAL");

  // If a previous profiling phase is still winding down, wait for it to
  // complete. We can't use task posting for this coordination because the
  // thread owning the profiler may not have a message loop.
  if (!profiling_inactive_.IsSignaled())
    profiling_inactive_.Wait();
  profiling_inactive_.Reset();

  DCHECK_EQ(kNullProfilerId, profiler_id_);
  profiler_id_ = SamplingThread::GetInstance()->Add(
      std::make_unique<SamplingThread::CollectionContext>(
          thread_id_, params_, &profiling_inactive_, std::move(native_sampler),
          std::move(profile_builder_)));
  DCHECK_NE(kNullProfilerId, profiler_id_);
}

void StackSamplingProfiler::Stop() {
  SamplingThread::GetInstance()->Remove(profiler_id_);
  profiler_id_ = kNullProfilerId;
}

// static
void StackSamplingProfiler::SetProcessMilestone(int milestone) {
  DCHECK_LE(0, milestone);
  DCHECK_GT(static_cast<int>(sizeof(process_milestones_) * 8), milestone);
  DCHECK_EQ(0, subtle::NoBarrier_Load(&process_milestones_) & (1 << milestone));
  ChangeAtomicFlags(&process_milestones_, 1 << milestone, 0);
}

// static
subtle::Atomic32 StackSamplingProfiler::ProcessMilestone() {
  return subtle::NoBarrier_Load(&process_milestones_);
}

// StackSamplingProfiler::Frame global functions ------------------------------

bool operator==(const StackSamplingProfiler::Module& a,
                const StackSamplingProfiler::Module& b) {
  return a.base_address == b.base_address && a.id == b.id &&
         a.filename == b.filename;
}

bool operator==(const StackSamplingProfiler::Sample& a,
                const StackSamplingProfiler::Sample& b) {
  return a.process_milestones == b.process_milestones && a.frames == b.frames;
}

bool operator!=(const StackSamplingProfiler::Sample& a,
                const StackSamplingProfiler::Sample& b) {
  return !(a == b);
}

bool operator<(const StackSamplingProfiler::Sample& a,
               const StackSamplingProfiler::Sample& b) {
  if (a.process_milestones != b.process_milestones)
    return a.process_milestones < b.process_milestones;

  return a.frames < b.frames;
}

bool operator==(const StackSamplingProfiler::Frame& a,
                const StackSamplingProfiler::Frame& b) {
  return a.instruction_pointer == b.instruction_pointer &&
         a.module_index == b.module_index;
}

bool operator<(const StackSamplingProfiler::Frame& a,
               const StackSamplingProfiler::Frame& b) {
  if (a.module_index != b.module_index)
    return a.module_index < b.module_index;

  return a.instruction_pointer < b.instruction_pointer;
}

}  // namespace base
