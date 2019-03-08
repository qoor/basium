// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_MODULE_CACHE_H_
#define BASE_SAMPLING_HEAP_PROFILER_MODULE_CACHE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

// Supports cached lookup of modules by address, with caching based on module
// address ranges.
//
// Cached lookup is necessary on Mac for performance, due to an inefficient
// dladdr implementation. See https://crrev.com/487092.
//
// Cached lookup is beneficial on Windows to minimize use of the loader
// lock. Note however that the cache retains a handle to looked-up modules for
// its lifetime, which may result in pinning modules in memory that were
// transiently loaded by the OS.
class BASE_EXPORT ModuleCache {
 public:
  // Module represents a binary module (executable or library) and its
  // associated state.
  class BASE_EXPORT Module {
   public:
    Module() = default;
    virtual ~Module() = default;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    // Gets the base address of the module.
    virtual uintptr_t GetBaseAddress() const = 0;

    // Gets the opaque binary string that uniquely identifies a particular
    // program version with high probability. This is parsed from headers of the
    // loaded module.
    // For binaries generated by GNU tools:
    //   Contents of the .note.gnu.build-id field.
    // On Windows:
    //   GUID + AGE in the debug image headers of a module.
    virtual std::string GetId() const = 0;

    // Gets the debug basename of the module. This is the basename of the PDB
    // file on Windows and the basename of the binary on other platforms.
    virtual FilePath GetDebugBasename() const = 0;

    // Gets the size of the module.
    virtual size_t GetSize() const = 0;
  };

  ModuleCache();
  ~ModuleCache();

  // Gets the module containing |address| or nullptr if |address| is not within
  // a module. The returned module remains owned by and has the same lifetime as
  // the ModuleCache object.
  const Module* GetModuleForAddress(uintptr_t address);
  std::vector<const Module*> GetModules() const;

 private:
  // TODO(alph): Refactor corresponding functions to use public API instead,
  // and drop friends.

  // Creates a Module object for the specified memory address. Returns null if
  // the address does not belong to a module.
  static std::unique_ptr<Module> CreateModuleForAddress(uintptr_t address);
  friend class NativeStackSamplerMac;

#if defined(OS_MACOSX)
  // Returns the size of the _TEXT segment of the module loaded
  // at |module_addr|.
  static size_t GetModuleTextSize(const void* module_addr);
  friend bool MayTriggerUnwInitLocalCrash(uint64_t);
#endif

  std::map<uintptr_t, std::unique_ptr<Module>> modules_cache_map_;
};

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_MODULE_CACHE_H_
