// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
#define BASE_PROFILER_STACK_SAMPLING_PROFILER_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

class NativeStackSampler;
class NativeStackSamplerTestDelegate;

// StackSamplingProfiler periodically stops a thread to sample its stack, for
// the purpose of collecting information about which code paths are
// executing. This information is used in aggregate by UMA to identify hot
// and/or janky code paths.
//
// Sample StackSamplingProfiler usage:
//
//   // Create and customize params as desired.
//   base::StackStackSamplingProfiler::SamplingParams params;
//   // Any thread's ID may be passed as the target.
//   base::StackSamplingProfiler profiler(base::PlatformThread::CurrentId()),
//       params);
//
//   // Or, to process the profiles within Chrome rather than via UMA, use a
//   // custom completed callback:
//   base::StackStackSamplingProfiler::CompletedCallback
//       thread_safe_callback = ...;
//   base::StackSamplingProfiler profiler(base::PlatformThread::CurrentId()),
//       params, thread_safe_callback);
//
//   profiler.Start();
//   // ... work being done on the target thread here ...
//   profiler.Stop();  // optional, stops collection before complete per params
//
// The default SamplingParams causes stacks to be recorded in a single burst at
// a 10Hz interval for a total of 30 seconds. All of these parameters may be
// altered as desired.
//
// When all call stack profiles are complete, or the profiler is stopped, the
// completed callback is called from a thread created by the profiler with the
// collected profiles.
//
// The results of the profiling are passed to the completed callback and consist
// of a vector of CallStackProfiles. Each CallStackProfile corresponds to a
// burst as specified in SamplingParams and contains a set of Samples and
// Modules. One Sample corresponds to a single recorded stack, and the Modules
// record those modules associated with the recorded stack frames.
class BASE_EXPORT StackSamplingProfiler {
 public:
  // Module represents the module (DLL or exe) corresponding to a stack frame.
  struct BASE_EXPORT Module {
    Module();
    Module(uintptr_t base_address,
           const std::string& id,
           const FilePath& filename);
    ~Module();

    // Points to the base address of the module.
    uintptr_t base_address;

    // An opaque binary string that uniquely identifies a particular program
    // version with high probability. This is parsed from headers of the loaded
    // module.
    // For binaries generated by GNU tools:
    //   Contents of the .note.gnu.build-id field.
    // On Windows:
    //   GUID + AGE in the debug image headers of a module.
    std::string id;

    // The filename of the module.
    FilePath filename;
  };

  // Frame represents an individual sampled stack frame with module information.
  struct BASE_EXPORT Frame {
    // Identifies an unknown module.
    static const size_t kUnknownModuleIndex = static_cast<size_t>(-1);

    Frame(uintptr_t instruction_pointer, size_t module_index);
    ~Frame();

    // Default constructor to satisfy IPC macros. Do not use explicitly.
    Frame();

    // The sampled instruction pointer within the function.
    uintptr_t instruction_pointer;

    // Index of the module in CallStackProfile::modules. We don't represent
    // module state directly here to save space.
    size_t module_index;
  };

  // Sample represents a set of stack frames.
  using Sample = std::vector<Frame>;

  // CallStackProfile represents a set of samples.
  struct BASE_EXPORT CallStackProfile {
    CallStackProfile();
    ~CallStackProfile();

    std::vector<Module> modules;
    std::vector<Sample> samples;

    // Duration of this profile.
    TimeDelta profile_duration;

    // Time between samples.
    TimeDelta sampling_period;
  };

  using CallStackProfiles = std::vector<CallStackProfile>;

  // Represents parameters that configure the sampling.
  struct BASE_EXPORT SamplingParams {
    SamplingParams();

    // Time to delay before first samples are taken. Defaults to 0.
    TimeDelta initial_delay;

    // Number of sampling bursts to perform. Defaults to 1.
    int bursts;

    // Interval between sampling bursts. This is the desired duration from the
    // start of one burst to the start of the next burst. Defaults to 10s.
    TimeDelta burst_interval;

    // Number of samples to record per burst. Defaults to 300.
    int samples_per_burst;

    // Interval between samples during a sampling burst. This is the desired
    // duration from the start of one sample to the start of the next
    // sample. Defaults to 100ms.
    TimeDelta sampling_interval;
  };

  // The callback type used to collect completed profiles.
  //
  // IMPORTANT NOTE: the callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler and
  // set the callback, and thus the callback must be callable on any thread. For
  // threads with message loops that create StackSamplingProfilers, posting a
  // task to the message loop with a copy of the profiles is the recommended
  // thread-safe callback implementation.
  using CompletedCallback = Callback<void(const CallStackProfiles&)>;

  // Creates a profiler that sends completed profiles to |callback|. The second
  // constructor is for test purposes.
  StackSamplingProfiler(PlatformThreadId thread_id,
                        const SamplingParams& params,
                        const CompletedCallback& callback);
  StackSamplingProfiler(PlatformThreadId thread_id,
                        const SamplingParams& params,
                        const CompletedCallback& callback,
                        NativeStackSamplerTestDelegate* test_delegate);
  // Stops any profiling currently taking place before destroying the profiler.
  ~StackSamplingProfiler();

  // The fire-and-forget interface: starts a profiler and allows it to complete
  // without the caller needing to manage the profiler lifetime. May be invoked
  // from any thread, but requires that the calling thread has a message loop.
  static void StartAndRunAsync(PlatformThreadId thread_id,
                               const SamplingParams& params,
                               const CompletedCallback& callback);

  // Initializes the profiler and starts sampling.
  void Start();

  // Stops the profiler and any ongoing sampling. Calling this function is
  // optional; if not invoked profiling terminates when all the profiling bursts
  // specified in the SamplingParams are completed or the profiler is destroyed,
  // whichever occurs first.
  void Stop();

 private:
  // SamplingThread is a separate thread used to suspend and sample stacks from
  // the target thread.
  class SamplingThread : public PlatformThread::Delegate {
   public:
    // Samples stacks using |native_sampler|. When complete, invokes
    // |completed_callback| with the collected call stack profiles.
    // |completed_callback| must be callable on any thread.
    SamplingThread(scoped_ptr<NativeStackSampler> native_sampler,
                   const SamplingParams& params,
                   const CompletedCallback& completed_callback);
    ~SamplingThread() override;

    // PlatformThread::Delegate:
    void ThreadMain() override;

    void Stop();

   private:
    // Collects |profile| from a single burst. If the profiler was stopped
    // during collection, sets |was_stopped| and provides the set of samples
    // collected up to that point.
    void CollectProfile(CallStackProfile* profile, TimeDelta* elapsed_time,
                        bool* was_stopped);

    // Collects call stack profiles from all bursts, or until the sampling is
    // stopped. If stopped before complete, the last profile in
    // |call_stack_profiles| may contain a partial burst.
    void CollectProfiles(CallStackProfiles* profiles);

    scoped_ptr<NativeStackSampler> native_sampler_;
    const SamplingParams params_;

    // If Stop() is called, it signals this event to force the sampling to
    // terminate before all the samples specified in |params_| are collected.
    WaitableEvent stop_event_;

    const CompletedCallback completed_callback_;

    DISALLOW_COPY_AND_ASSIGN(SamplingThread);
  };

  // The thread whose stack will be sampled.
  PlatformThreadId thread_id_;

  const SamplingParams params_;

  scoped_ptr<SamplingThread> sampling_thread_;
  PlatformThreadHandle sampling_thread_handle_;

  const CompletedCallback completed_callback_;

  // Stored until it can be passed to the NativeStackSampler created in Start().
  NativeStackSamplerTestDelegate* const test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplingProfiler);
};

// The metrics provider code wants to put Samples in a map and compare them,
// which requires us to define a few operators.
BASE_EXPORT bool operator==(const StackSamplingProfiler::Frame& a,
                            const StackSamplingProfiler::Frame& b);
BASE_EXPORT bool operator<(const StackSamplingProfiler::Frame& a,
                           const StackSamplingProfiler::Frame& b);

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
