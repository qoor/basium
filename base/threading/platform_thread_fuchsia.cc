// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <pthread.h>
#include <sched.h>

#include "base/threading/platform_thread_internal_posix.h"

namespace base {

namespace internal {

const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4] = {
    {ThreadPriority::BACKGROUND, 10},
    {ThreadPriority::NORMAL, 0},
    {ThreadPriority::DISPLAY, -8},
    {ThreadPriority::REALTIME_AUDIO, -10},
};

bool SetCurrentThreadPriorityForPlatform(ThreadPriority priority) {
  sched_param prio = {0};
  prio.sched_priority = ThreadPriorityToNiceValue(priority);
  return pthread_setschedparam(pthread_self(), SCHED_OTHER, &prio) == 0;
}

bool GetCurrentThreadPriorityForPlatform(ThreadPriority* priority) {
  sched_param prio = {0};
  int policy;
  if (pthread_getschedparam(pthread_self(), &policy, &prio) != 0) {
    return false;
  }
  *priority = NiceValueToThreadPriority(prio.sched_priority);
  return true;
}

}  // namespace internal

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

// static
void PlatformThread::SetName(const std::string& name) {
  // TODO(fuchsia): There's no available API at the moment to set the thread
  // name. See https://crbug.com/725726.
}

}  // namespace base
