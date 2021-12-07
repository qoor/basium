// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_BACKUP_UTIL_H_
#define BASE_MAC_BACKUP_UTIL_H_

#include "base/base_export.h"

namespace base {

class FilePath;

namespace mac {

// Returns true if the file or directory at `file_path` is excluded from
// OS-managed backups.
BASE_EXPORT bool GetBackupExclusion(const FilePath& file_path);

// Excludes the file or directory given by `file_path` from OS-managed backups.
BASE_EXPORT bool SetBackupExclusion(const FilePath& file_path);

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_BACKUP_UTIL_H_
