// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_
#define BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {

namespace debug {

// Handler to silently dump the current process without crashing.
// Before calling this function, call SetDumpWithoutCrashingFunction to pass a
// function pointer.
// Windows:
// This must be done for each instance of base (i.e. module) and is normally
// chrome_elf!DumpProcessWithoutCrash. See example code in chrome_main.cc that
// does this for chrome.dll and chrome_child.dll. Note: Crashpad sets this up
// for main chrome.exe as part of calling crash_reporter::InitializeCrashpad.
// Mac/Linux:
// Crashpad does this as part of crash_reporter::InitializeCrashpad.
// Returns false if called before SetDumpWithoutCrashingFunction.
//
// This function must not be called with a tail-call because that would cause
// the caller to be omitted from the call stack in the crash dump, and that is
// confusing and omits what is likely the most important context.
#if defined(__clang__) && __has_attribute(not_tail_called)
BASE_EXPORT bool __attribute__((not_tail_called)) DumpWithoutCrashing();
#else
BASE_EXPORT bool DumpWithoutCrashing();
#endif

// Sets a function that'll be invoked to dump the current process when
// DumpWithoutCrashing() is called.
BASE_EXPORT void SetDumpWithoutCrashingFunction(void (CDECL *function)());

}  // namespace debug

}  // namespace base

#endif  // BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_
