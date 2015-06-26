// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_

#include <vector>

#include "base/atomicops.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"

namespace base {

class SingleThreadTaskRunner;

namespace trace_event {

namespace {
class ProcessMemoryDumpHolder;
}

class MemoryDumpManagerDelegate;
class MemoryDumpProvider;
class ProcessMemoryDump;
class MemoryDumpSessionState;

// This is the interface exposed to the rest of the codebase to deal with
// memory tracing. The main entry point for clients is represented by
// RequestDumpPoint(). The extension by Un(RegisterDumpProvider).
class BASE_EXPORT MemoryDumpManager : public TraceLog::EnabledStateObserver {
 public:
  static const uint64 kInvalidTracingProcessId = 0;
  static const char* const kTraceCategoryForTesting;

  static MemoryDumpManager* GetInstance();

  // Invoked once per process to register the TraceLog observer.
  void Initialize();

  // See the lifetime and thread-safety requirements on the delegate below in
  // the |MemoryDumpManagerDelegate| docstring.
  void SetDelegate(MemoryDumpManagerDelegate* delegate);

  // MemoryDumpManager does NOT take memory ownership of |mdp|, which is
  // expected to either be a singleton or unregister itself.
  // If the optional |task_runner| argument is non-null, all the calls to the
  // |mdp| will be issues on the given thread. Otherwise, the |mdp| should be
  // able to handle calls on arbitrary threads.
  void RegisterDumpProvider(
      MemoryDumpProvider* mdp,
      const scoped_refptr<SingleThreadTaskRunner>& task_runner);
  void RegisterDumpProvider(MemoryDumpProvider* mdp);
  void UnregisterDumpProvider(MemoryDumpProvider* mdp);

  // Requests a memory dump. The dump might happen or not depending on the
  // filters and categories specified when enabling tracing.
  // The optional |callback| is executed asynchronously, on an arbitrary thread,
  // to notify about the completion of the global dump (i.e. after all the
  // processes have dumped) and its success (true iff all the dumps were
  // successful).
  void RequestGlobalDump(MemoryDumpType dump_type,
                         const MemoryDumpCallback& callback);

  // Same as above (still asynchronous), but without callback.
  void RequestGlobalDump(MemoryDumpType dump_type);

  // TraceLog::EnabledStateObserver implementation.
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  // Returns the MemoryDumpSessionState object, which is shared by all the
  // ProcessMemoryDump and MemoryAllocatorDump instances through all the tracing
  // session lifetime.
  const scoped_refptr<MemoryDumpSessionState>& session_state() const {
    return session_state_;
  }

  // Derives a tracing process id from a child process id. Child process ids
  // cannot be used directly in tracing for security reasons (see: discussion in
  // crrev.com/1173263004). This method is meant to be used when dumping
  // cross-process shared memory from a process which knows the child process id
  // of its endpoints. The value returned by this method is guaranteed to be
  // equal to the value returned by tracing_process_id() in the corresponding
  // child process.
  // This will never return kInvalidTracingProcessId.
  static uint64 ChildProcessIdToTracingProcessId(int child_id);

  // Returns a unique id for the current process. The id can be retrieved only
  // by child processes and only when tracing is enabled. This is intended to
  // express cross-process sharing of memory dumps on the child-process side,
  // without having to know its own child process id.
  uint64 tracing_process_id() const {
    DCHECK_NE(kInvalidTracingProcessId, tracing_process_id_);
    return tracing_process_id_;
  }

 private:
  // Descriptor struct used to hold information about registered MDPs. It is
  // deliberately copyable, in order to allow to be used as hash_map value.
  struct MemoryDumpProviderInfo {
    MemoryDumpProviderInfo(
        const scoped_refptr<SingleThreadTaskRunner>& task_runner);
    ~MemoryDumpProviderInfo();

    scoped_refptr<SingleThreadTaskRunner> task_runner;  // Optional.
    int consecutive_failures;  // Number of times the provider failed (to
                               // disable the MDPs).
    bool disabled;  // For fail-safe logic (auto-disable failing MDPs).
  };

  friend struct DefaultDeleter<MemoryDumpManager>;  // For the testing instance.
  friend struct DefaultSingletonTraits<MemoryDumpManager>;
  friend class MemoryDumpManagerDelegate;
  friend class MemoryDumpManagerTest;
  FRIEND_TEST_ALL_PREFIXES(MemoryDumpManagerTest, DisableFailingDumpers);

  static const int kMaxConsecutiveFailuresCount;

  static void SetInstanceForTesting(MemoryDumpManager* instance);

  MemoryDumpManager();
  virtual ~MemoryDumpManager();

  // Internal, used only by MemoryDumpManagerDelegate.
  // Creates a memory dump for the current process and appends it to the trace.
  // |callback| will be invoked asynchronously upon completion on the same
  // thread on which CreateProcessDump() was called.
  void CreateProcessDump(const MemoryDumpRequestArgs& args,
                         const MemoryDumpCallback& callback);

  bool InvokeDumpProviderLocked(MemoryDumpProvider* mdp,
                                ProcessMemoryDump* pmd);
  void ContinueAsyncProcessDump(
      MemoryDumpProvider* mdp,
      scoped_refptr<ProcessMemoryDumpHolder> pmd_holder);

  // Pass kInvalidTracingProcessId for invalidating the id.
  void set_tracing_process_id(uint64 id) {
    DCHECK(tracing_process_id_ == kInvalidTracingProcessId ||
           id == kInvalidTracingProcessId || tracing_process_id_ == id);
    tracing_process_id_ = id;
  }

  hash_map<MemoryDumpProvider*, MemoryDumpProviderInfo> dump_providers_;

  // Shared among all the PMDs to keep state scoped to the tracing session.
  scoped_refptr<MemoryDumpSessionState> session_state_;

  MemoryDumpManagerDelegate* delegate_;  // Not owned.

  // Protects from concurrent accesses to the |dump_providers_*| and |delegate_|
  // to guard against disabling logging while dumping on another thread.
  Lock lock_;

  // Optimization to avoid attempting any memory dump (i.e. to not walk an empty
  // dump_providers_enabled_ list) when tracing is not enabled.
  subtle::AtomicWord memory_tracing_enabled_;

  // For time-triggered periodic dumps.
  RepeatingTimer<MemoryDumpManager> periodic_dump_timer_;

  // The unique id of the child process. This is created only for tracing and is
  // expected to be valid only when tracing is enabled.
  uint64 tracing_process_id_;

  // Skips the auto-registration of the core dumpers during Initialize().
  bool skip_core_dumpers_auto_registration_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManager);
};

// The delegate is supposed to be long lived (read: a Singleton) and thread
// safe (i.e. should expect calls from any thread and handle thread hopping).
class BASE_EXPORT MemoryDumpManagerDelegate {
 public:
  virtual void RequestGlobalMemoryDump(const MemoryDumpRequestArgs& args,
                                       const MemoryDumpCallback& callback) = 0;

  // Determines whether the MemoryDumpManager instance should be the master
  // (the ones which initiates and coordinates the multiprocess dumps) or not.
  virtual bool IsCoordinatorProcess() const = 0;

 protected:
  MemoryDumpManagerDelegate() {}
  virtual ~MemoryDumpManagerDelegate() {}

  void CreateProcessDump(const MemoryDumpRequestArgs& args,
                         const MemoryDumpCallback& callback) {
    MemoryDumpManager::GetInstance()->CreateProcessDump(args, callback);
  }

  void set_tracing_process_id(uint64 id) {
    MemoryDumpManager::GetInstance()->set_tracing_process_id(id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManagerDelegate);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
