// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_UTIL_PROXY_H_
#define BASE_FILES_FILE_UTIL_PROXY_H_

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace base {

class TaskRunner;
class Time;

// This class provides asynchronous access to common file routines.
class BASE_EXPORT FileUtilProxy {
 public:
  // This callback is used by methods that report only an error code.  It is
  // valid to pass a null callback to any function that takes a StatusCallback,
  // in which case the operation will complete silently.
  using StatusCallback = OnceCallback<void(File::Error)>;

  using GetFileInfoCallback =
      OnceCallback<void(File::Error, const File::Info&)>;

  // Retrieves the information about a file. It is invalid to pass a null
  // callback.
  // This returns false if task posting to |task_runner| has failed.
  static bool GetFileInfo(TaskRunner* task_runner,
                          const FilePath& file_path,
                          GetFileInfoCallback callback);

  // Touches a file. The callback can be null.
  // This returns false if task posting to |task_runner| has failed.
  static bool Touch(TaskRunner* task_runner,
                    const FilePath& file_path,
                    const Time& last_access_time,
                    const Time& last_modified_time,
                    StatusCallback callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilProxy);
};

}  // namespace base

#endif  // BASE_FILES_FILE_UTIL_PROXY_H_
