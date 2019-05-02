// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
#define BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/process/launch.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_result.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

namespace base {

class CommandLine;
struct LaunchOptions;
class TestLauncher;

// Constants for GTest command-line flags.
extern const char kGTestFilterFlag[];
extern const char kGTestFlagfileFlag[];
extern const char kGTestHelpFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];
extern const char kGTestShuffleFlag[];
extern const char kGTestRandomSeedFlag[];
extern const char kIsolatedScriptRunDisabledTestsFlag[];
extern const char kIsolatedScriptTestFilterFlag[];
extern const char kIsolatedScriptTestRepeatFlag[];

// Interface for use with LaunchTests that abstracts away exact details
// which tests and how are run.
class TestLauncherDelegate {
 public:
  // Called to get names of tests available for running. The delegate
  // must put the result in |output| and return true on success.
  virtual bool GetTests(std::vector<TestIdentifier>* output) = 0;

  // Called before a test is considered for running. This method must return
  // true if either the delegate or the TestLauncher will run the test.
  virtual bool WillRunTest(const std::string& test_case_name,
                           const std::string& test_name) = 0;

  // Called before a test is considered for running. This method must return
  // true if the TestLauncher is expected to run the test, provided it is part
  // of the current shard.
  virtual bool ShouldRunTest(const std::string& test_case_name,
                             const std::string& test_name) = 0;

  // Called to make the delegate run the specified tests. The delegate must
  // return the number of actual tests it's going to run (can be smaller,
  // equal to, or larger than size of |test_names|). It must also call
  // |test_launcher|'s OnTestFinished method once per every run test,
  // regardless of its success.
  virtual size_t RunTests(TestLauncher* test_launcher,
                          const std::vector<std::string>& test_names) = 0;

  // Called to make the delegate retry the specified tests. The delegate must
  // return the number of actual tests it's going to retry (can be smaller,
  // equal to, or larger than size of |test_names|). It must also call
  // |test_launcher|'s OnTestFinished method once per every retried test,
  // regardless of its success.
  virtual size_t RetryTests(TestLauncher* test_launcher,
                            const std::vector<std::string>& test_names) = 0;

  virtual ~TestLauncherDelegate();
};

// An observer of child process lifetime events generated by
// LaunchChildGTestProcess.
class ProcessLifetimeObserver {
 public:
  virtual ~ProcessLifetimeObserver() = default;

  // Invoked once the child process is started. |handle| is a handle to the
  // child process and |id| is its pid. NOTE: this method is invoked on the
  // thread the process is launched on immediately after it is launched. The
  // caller owns the ProcessHandle.
  virtual void OnLaunched(ProcessHandle handle, ProcessId id) {}

  // Invoked when a test process exceeds its runtime, immediately before it is
  // terminated. |command_line| is the command line used to launch the process.
  // NOTE: this method is invoked on the thread the process is launched on.
  virtual void OnTimedOut(const CommandLine& command_line) {}

  // Invoked after a child process finishes, reporting the process |exit_code|,
  // child process |elapsed_time|, whether or not the process was terminated as
  // a result of a timeout, and the output of the child (stdout and stderr
  // together). NOTE: this method is invoked on the same thread as
  // LaunchChildGTestProcess.
  virtual void OnCompleted(int exit_code,
                           TimeDelta elapsed_time,
                           bool was_timeout,
                           const std::string& output) {}

 protected:
  ProcessLifetimeObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessLifetimeObserver);
};

// Launches tests using a TestLauncherDelegate.
class TestLauncher {
 public:
  // Flags controlling behavior of LaunchChildGTestProcess.
  enum LaunchChildGTestProcessFlags {
    // Allows usage of job objects on Windows. Helps properly clean up child
    // processes.
    USE_JOB_OBJECTS = (1 << 0),

    // Allows breakaway from job on Windows. May result in some child processes
    // not being properly terminated after launcher dies if these processes
    // fail to cooperate.
    ALLOW_BREAKAWAY_FROM_JOB = (1 << 1),
  };

  struct LaunchOptions {
    LaunchOptions();
    LaunchOptions(const LaunchOptions& other);
    ~LaunchOptions();

    int flags = 0;
    // These mirror values in base::LaunchOptions, see it for details.
#if defined(OS_WIN)
    base::LaunchOptions::Inherit inherit_mode =
        base::LaunchOptions::Inherit::kSpecific;
    base::HandlesToInheritVector handles_to_inherit;
#else
    FileHandleMappingVector fds_to_remap;
#endif
  };

  // Constructor. |parallel_jobs| is the limit of simultaneous parallel test
  // jobs.
  TestLauncher(TestLauncherDelegate* launcher_delegate, size_t parallel_jobs);
  // virtual to mock in testing.
  virtual ~TestLauncher();

  // Runs the launcher. Must be called at most once.
  bool Run() WARN_UNUSED_RESULT;

  // Launches a child process (assumed to be gtest-based binary) using
  // |command_line|. If |wrapper| is not empty, it is prepended to the final
  // command line. |observer|, if not null, is used to convey process lifetime
  // events to the caller. |observer| is destroyed after its OnCompleted
  // method is invoked.
  // virtual to mock in testing.
  virtual void LaunchChildGTestProcess(
      const CommandLine& command_line,
      const std::string& wrapper,
      TimeDelta timeout,
      const LaunchOptions& options,
      std::unique_ptr<ProcessLifetimeObserver> observer);

  // Called when a test has finished running.
  void OnTestFinished(const TestResult& result);

 private:
  bool Init() WARN_UNUSED_RESULT;

  // Runs all tests in current iteration.
  void RunTests();

  void CombinePositiveTestFilters(std::vector<std::string> filter_a,
                                  std::vector<std::string> filter_b);

  void RunTestIteration();

#if defined(OS_POSIX)
  void OnShutdownPipeReadable();
#endif

  // Saves test results summary as JSON if requested from command line.
  void MaybeSaveSummaryAsJSON(const std::vector<std::string>& additional_tags);

  // Called when a test iteration is finished.
  void OnTestIterationFinished();

  // Called by the delay timer when no output was made for a while.
  void OnOutputTimeout();

  // Make sure we don't accidentally call the wrong methods e.g. on the worker
  // pool thread.  Should be the first member so that it's destroyed last: when
  // destroying other members, especially the worker pool, we may check the code
  // is running on the correct thread.
  ThreadChecker thread_checker_;

  TestLauncherDelegate* launcher_delegate_;

  // Support for outer sharding, just like gtest does.
  int32_t total_shards_;  // Total number of outer shards, at least one.
  int32_t shard_index_;   // Index of shard the launcher is to run.

  int cycles_;  // Number of remaining test iterations, or -1 for infinite.

  // Test filters (empty means no filter).
  bool has_at_least_one_positive_filter_;
  std::vector<std::string> positive_test_filter_;
  std::vector<std::string> negative_test_filter_;

  // Tests to use (cached result of TestLauncherDelegate::GetTests).
  std::vector<TestIdentifier> tests_;

  // Number of tests found in this binary.
  size_t test_found_count_;

  // Number of tests started in this iteration.
  size_t test_started_count_;

  // Number of tests finished in this iteration.
  size_t test_finished_count_;

  // Number of tests successfully finished in this iteration.
  size_t test_success_count_;

  // Number of tests either timing out or having an unknown result,
  // likely indicating a more systemic problem if widespread.
  size_t test_broken_count_;

  // Number of retries in this iteration.
  size_t retry_count_;

  // Maximum number of retries per iteration.
  size_t retry_limit_;

  // If true will not early exit nor skip retries even if too many tests are
  // broken.
  bool force_run_broken_tests_;

  // Tests to retry in this iteration.
  std::set<std::string> tests_to_retry_;

  // Result to be returned from Run.
  bool run_result_;

  // Support for test shuffling, just like gtest does.
  bool shuffle_;
  uint32_t shuffle_seed_;

  TestResultsTracker results_tracker_;

  // Watchdog timer to make sure we do not go without output for too long.
  DelayTimer watchdog_timer_;

  // Number of jobs to run in parallel.
  size_t parallel_jobs_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncher);
};

// Return the number of parallel jobs to use, or 0U in case of error.
size_t NumParallelJobs();

// Extract part from |full_output| that applies to |result|.
std::string GetTestOutputSnippet(const TestResult& result,
                                 const std::string& full_output);

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
