// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/hang_watcher.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"

namespace base {

namespace {
HangWatcher* g_instance = nullptr;
}

constexpr const char* kThreadName = "HangWatcher";

// The time that the HangWatcher thread will sleep for between calls to
// Monitor(). Increasing or decreasing this does not modify the type of hangs
// that can be detected. It instead increases the probability that a call to
// Monitor() will happen at the right time to catch a hang. This has to be
// balanced with power/cpu use concerns as busy looping would catch amost all
// hangs but present unacceptable overhead.
const base::TimeDelta kMonitoringPeriod = base::TimeDelta::FromSeconds(10);

HangWatchScope::HangWatchScope(TimeDelta timeout) {
  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

  DCHECK(current_hang_watch_state)
      << "A scope can only be used on a thread that "
         "registered for hang watching with HangWatcher::RegisterThread.";

#if DCHECK_IS_ON()
  previous_scope_ = current_hang_watch_state->GetCurrentHangWatchScope();
  current_hang_watch_state->SetCurrentHangWatchScope(this);
#endif

  // TODO(crbug.com/1034046): Check whether we are over deadline already for the
  // previous scope here by issuing only one TimeTicks::Now() and resuing the
  // value.

  previous_deadline_ = current_hang_watch_state->GetDeadline();
  TimeTicks deadline = TimeTicks::Now() + timeout;
  current_hang_watch_state->SetDeadline(deadline);
}

HangWatchScope::~HangWatchScope() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

#if DCHECK_IS_ON()
  // Verify that no Scope was destructed out of order.
  DCHECK_EQ(this, current_hang_watch_state->GetCurrentHangWatchScope());
  current_hang_watch_state->SetCurrentHangWatchScope(previous_scope_);
#endif

  // Reset the deadline to the value it had before entering this scope.
  current_hang_watch_state->SetDeadline(previous_deadline_);
  // TODO(crbug.com/1034046): Log when a HangWatchScope exits after its deadline
  // and that went undetected by the HangWatcher.
}

HangWatcher::HangWatcher(RepeatingClosure on_hang_closure)
    : monitor_period_(kMonitoringPeriod),
      monitor_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                     WaitableEvent::InitialState::NOT_SIGNALED),
      on_hang_closure_(std::move(on_hang_closure)),
      thread_(this, kThreadName) {
  // |thread_checker_| should not be bound to the constructing thread.
  DETACH_FROM_THREAD(thread_checker_);

  DCHECK(!g_instance);
  g_instance = this;
  Start();
}

HangWatcher::~HangWatcher() {
  DCHECK_EQ(g_instance, this);
  DCHECK(watch_states_.empty());
  g_instance = nullptr;
  Stop();
}

void HangWatcher::Start() {
  thread_.Start();
}

void HangWatcher::Stop() {
  keep_monitoring_.store(false);
  monitor_event_.Signal();
  thread_.Join();
}

bool HangWatcher::IsWatchListEmpty() {
  AutoLock auto_lock(watch_state_lock_);
  return watch_states_.empty();
}

void HangWatcher::Run() {
  // Monitor() should only run on |thread_|. Bind |thread_checker_| here to make
  // sure of that.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (keep_monitoring_) {
    // If there is nothing to watch sleep until there is.
    if (IsWatchListEmpty()) {
      monitor_event_.Wait();
    } else {
      Monitor();
    }

    if (keep_monitoring_) {
      // Sleep until next scheduled monitoring.
      monitor_event_.TimedWait(monitor_period_);
    }
  }
}

// static
HangWatcher* HangWatcher::GetInstance() {
  return g_instance;
}

ScopedClosureRunner HangWatcher::RegisterThread() {
  AutoLock auto_lock(watch_state_lock_);

  watch_states_.push_back(
      internal::HangWatchState::CreateHangWatchStateForCurrentThread());

  // Now that there is a thread to monitor we wake the HangWatcher thread.
  if (watch_states_.size() == 1) {
    monitor_event_.Signal();
  }

  return ScopedClosureRunner(BindOnce(&HangWatcher::UnregisterThread,
                                      Unretained(HangWatcher::GetInstance())));
}

void HangWatcher::Monitor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool must_invoke_hang_closure = false;
  {
    AutoLock auto_lock(watch_state_lock_);
    for (const auto& watch_state : watch_states_) {
      if (watch_state->IsOverDeadline()) {
        must_invoke_hang_closure = true;
        break;
      }
    }
  }

  if (must_invoke_hang_closure) {
    // Invoke the closure outside the scope of |watch_state_lock_|
    // to prevent lock reentrancy.
    on_hang_closure_.Run();
  }

  if (after_monitor_closure_for_testing_) {
    after_monitor_closure_for_testing_.Run();
  }
}

void HangWatcher::SetAfterMonitorClosureForTesting(
    base::RepeatingClosure closure) {
  after_monitor_closure_for_testing_ = std::move(closure);
}

void HangWatcher::SetMonitoringPeriodForTesting(base::TimeDelta period) {
  monitor_period_ = period;
}

void HangWatcher::SignalMonitorEventForTesting() {
  monitor_event_.Signal();
}

void HangWatcher::UnregisterThread() {
  AutoLock auto_lock(watch_state_lock_);

  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

  auto it =
      std::find_if(watch_states_.cbegin(), watch_states_.cend(),
                   [current_hang_watch_state](
                       const std::unique_ptr<internal::HangWatchState>& state) {
                     return state.get() == current_hang_watch_state;
                   });

  // Thread should be registered to get unregistered.
  DCHECK(it != watch_states_.end());

  watch_states_.erase(it);
}

namespace internal {

// |deadline_| starts at Max() to avoid validation problems
// when setting the first legitimate value.
HangWatchState::HangWatchState() : deadline_(TimeTicks::Max()) {
  // There should not exist a state object for this thread already.
  DCHECK(!GetHangWatchStateForCurrentThread()->Get());

  // Bind the new instance to this thread.
  GetHangWatchStateForCurrentThread()->Set(this);
}

HangWatchState::~HangWatchState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(GetHangWatchStateForCurrentThread()->Get(), this);
  GetHangWatchStateForCurrentThread()->Set(nullptr);

#if DCHECK_IS_ON()
  // Destroying the HangWatchState should not be done if there are live
  // HangWatchScopes.
  DCHECK(!current_hang_watch_scope_);
#endif
}

// static
std::unique_ptr<HangWatchState>
HangWatchState::CreateHangWatchStateForCurrentThread() {

  // Allocate a watch state object for this thread.
  std::unique_ptr<HangWatchState> hang_state =
      std::make_unique<HangWatchState>();

  // Setting the thread local worked.
  DCHECK_EQ(GetHangWatchStateForCurrentThread()->Get(), hang_state.get());

  // Transfer ownership to caller.
  return hang_state;
}

TimeTicks HangWatchState::GetDeadline() const {
  return deadline_.load();
}

TimeTicks HangWatchState::SetDeadline(TimeTicks deadline) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return deadline_.exchange(deadline);
}

bool HangWatchState::IsOverDeadline() const {
  return TimeTicks::Now() > deadline_.load();
}

#if DCHECK_IS_ON()
void HangWatchState::SetCurrentHangWatchScope(HangWatchScope* scope) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  current_hang_watch_scope_ = scope;
}

HangWatchScope* HangWatchState::GetCurrentHangWatchScope() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_hang_watch_scope_;
}
#endif

// static
ThreadLocalPointer<HangWatchState>*
HangWatchState::GetHangWatchStateForCurrentThread() {
  static NoDestructor<ThreadLocalPointer<HangWatchState>> hang_watch_state;
  return hang_watch_state.get();
}

}  // namespace internal

}  // namespace base
