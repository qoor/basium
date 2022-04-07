// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/process.h"

namespace base {

bool PathProviderFuchsia(int key, FilePath* result) {
  switch (key) {
    case FILE_EXE:
      *result = CommandLine::ForCurrentProcess()->GetProgram();
      return true;
    case DIR_ASSETS:
      *result = base::FilePath(base::kPackageRootDirectoryPath);
      return true;
    case DIR_SRC_TEST_DATA_ROOT:
    case DIR_GEN_TEST_DATA_ROOT:
      // These are only used by tests.
      // Test binaries are added to the package root via GN deps.
      *result = base::FilePath(base::kPackageRootDirectoryPath);
      return true;
    case DIR_USER_DESKTOP:
      // TODO(crbug.com/1231928): Implement this case.
      NOTIMPLEMENTED_LOG_ONCE() << " for DIR_USER_DESKTOP.";
      return false;
    case DIR_HOME:
      // TODO(crbug.com/1231928) Provide a proper base::GetHomeDir()
      // implementation for Fuchsia and remove this case statement. See also
      // crbug.com/1261284. For now, log, return false, and let the base
      // implementation handle it. This will end up returning a temporary
      // directory.
      NOTIMPLEMENTED_LOG_ONCE() << "for DIR_HOME. Will use temporary dir.";
      return false;
  }

  return false;
}

}  // namespace base
