// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <fcntl.h>
#include <fdio/io.h>
#include <stdio.h>
#include <zircon/dlfcn.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include "base/base_paths_fuchsia.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_zx_handle.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  std::vector<base::FilePath::StringType> components;
  library_path.GetComponents(&components);
  if (components.size() != 1u) {
    NOTREACHED() << "library_path is a path, should be a filename: "
                 << library_path.MaybeAsASCII();
    return nullptr;
  }

  // Fuchsia libraries must live under the "lib" directory, which may be located
  // in /system/lib or /pkg/lib depending on whether the executable is running
  // inside a package.
  // TODO(https://crbug.com/805057): Remove the non-package codepath when bootfs
  // is deprecated.
  FilePath computed_path = base::GetPackageRoot();
  if (computed_path.empty()) {
    CHECK(PathService::Get(DIR_EXE, &computed_path));
  }
  computed_path = computed_path.AppendASCII("lib").Append(components[0]);
  base::File library(computed_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!library.IsValid()) {
    if (error) {
      error->message = base::StringPrintf(
          "open library: %s",
          base::File::ErrorToString(library.error_details()).c_str());
    }
    return nullptr;
  }

  base::ScopedZxHandle vmo;
  zx_status_t status =
      fdio_get_vmo_clone(library.GetPlatformFile(), vmo.receive());
  if (status != ZX_OK) {
    if (error) {
      error->message = base::StringPrintf("fdio_get_vmo_clone: %s",
                                          zx_status_get_string(status));
    }
    return nullptr;
  }
  NativeLibrary result = dlopen_vmo(vmo.get(), RTLD_LAZY | RTLD_LOCAL);
  return result;
}

void UnloadNativeLibrary(NativeLibrary library) {
  // dlclose() is a no-op on Fuchsia, so do nothing here.
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          StringPiece name) {
  return dlsym(library, name.data());
}

std::string GetNativeLibraryName(StringPiece name) {
  return base::StringPrintf("lib%s.so", name.as_string().c_str());
}

std::string GetLoadableModuleName(StringPiece name) {
  return GetNativeLibraryName(name);
}

}  // namespace base
