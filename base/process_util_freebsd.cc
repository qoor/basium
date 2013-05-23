// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process };

  if (sysctl(mib, arraysize(mib), &info, &length, NULL, 0) < 0)
    return -1;

  return info.ki_ppid;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  char pathname[PATH_MAX];
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, process };

  length = sizeof(pathname);

  if (sysctl(mib, arraysize(mib), pathname, &length, NULL, 0) < 0 ||
      length == 0) {
    return FilePath();
  }

  return FilePath(std::string(pathname));
}

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : index_of_kinfo_proc_(),
      filter_(filter) {

  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_UID, getuid() };

  bool done = false;
  int try_num = 1;
  const int max_tries = 10;

  do {
    size_t len = 0;
    if (sysctl(mib, arraysize(mib), NULL, &len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to get the size needed for the process list";
      kinfo_procs_.resize(0);
      done = true;
    } else {
      size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
      // Leave some spare room for process table growth (more could show up
      // between when we check and now)
      num_of_kinfo_proc += 16;
      kinfo_procs_.resize(num_of_kinfo_proc);
      len = num_of_kinfo_proc * sizeof(struct kinfo_proc);
      if (sysctl(mib, arraysize(mib), &kinfo_procs_[0], &len, NULL, 0) <0) {
        // If we get a mem error, it just means we need a bigger buffer, so
        // loop around again.  Anything else is a real error and give up.
        if (errno != ENOMEM) {
          LOG(ERROR) << "failed to get the process list";
          kinfo_procs_.resize(0);
          done = true;
        }
      } else {
        // Got the list, just make sure we're sized exactly right
        size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
        kinfo_procs_.resize(num_of_kinfo_proc);
        done = true;
      }
    }
  } while (!done && (try_num++ < max_tries));

  if (!done) {
    LOG(ERROR) << "failed to collect the process list in a few tries";
    kinfo_procs_.resize(0);
  }
}

ProcessIterator::~ProcessIterator() {
}

bool ProcessIterator::CheckForNextProcess() {
  std::string data;

  for (; index_of_kinfo_proc_ < kinfo_procs_.size(); ++index_of_kinfo_proc_) {
    size_t length;
    struct kinfo_proc kinfo = kinfo_procs_[index_of_kinfo_proc_];
    int mib[] = { CTL_KERN, KERN_PROC_ARGS, kinfo.ki_pid };

    if ((kinfo.ki_pid > 0) && (kinfo.ki_stat == SZOMB))
      continue;

    length = 0;
    if (sysctl(mib, arraysize(mib), NULL, &length, NULL, 0) < 0) {
      LOG(ERROR) << "failed to figure out the buffer size for a command line";
      continue;
    }

    data.resize(length);

    if (sysctl(mib, arraysize(mib), &data[0], &length, NULL, 0) < 0) {
      LOG(ERROR) << "failed to fetch a commandline";
      continue;
    }

    std::string delimiters;
    delimiters.push_back('\0');
    Tokenize(data, delimiters, &entry_.cmd_line_args_);

    size_t exec_name_end = data.find('\0');
    if (exec_name_end == std::string::npos) {
      LOG(ERROR) << "command line data didn't match expected format";
      continue;
    }

    entry_.pid_ = kinfo.ki_pid;
    entry_.ppid_ = kinfo.ki_ppid;
    entry_.gid_ = kinfo.ki_pgid;

    size_t last_slash = data.rfind('/', exec_name_end);
    if (last_slash == std::string::npos) {
      entry_.exe_file_.assign(data, 0, exec_name_end);
    } else {
      entry_.exe_file_.assign(data, last_slash + 1,
                              exec_name_end - last_slash - 1);
    }

    // Start w/ the next entry next time through
    ++index_of_kinfo_proc_;

    return true;
  }
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  if (executable_name_ != entry().exe_file())
    return false;

  return ProcessIterator::IncludeEntry();
}

void EnableTerminationOnOutOfMemory() {
  DLOG(WARNING) << "Not feasible.";
}

void EnableTerminationOnHeapCorruption() {
  // Nothing to do.
}

bool AdjustOOMScore(ProcessId process, int score) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
