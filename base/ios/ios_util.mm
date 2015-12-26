// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/macros.h"
#include "base/sys_info.h"

namespace {
// Return a 3 elements array containing the major, minor and bug fix version of
// the OS.
const int32_t* OSVersionAsArray() {
  int32_t* digits = new int32_t[3];
  base::SysInfo::OperatingSystemVersionNumbers(
      &digits[0], &digits[1], &digits[2]);
  return digits;
}
}  // namespace

namespace base {
namespace ios {

// When dropping iOS7 support, also address issues listed in crbug.com/502968.
bool IsRunningOnIOS8OrLater() {
  return IsRunningOnOrLater(8, 0, 0);
}

bool IsRunningOnIOS9OrLater() {
  return IsRunningOnOrLater(9, 0, 0);
}

bool IsRunningOnOrLater(int32_t major, int32_t minor, int32_t bug_fix) {
  static const int32_t* current_version = OSVersionAsArray();
  int32_t version[] = {major, minor, bug_fix};
  for (size_t i = 0; i < arraysize(version); i++) {
    if (current_version[i] != version[i])
      return current_version[i] > version[i];
  }
  return true;
}

bool IsInForcedRTL() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  return [defaults boolForKey:@"NSForceRightToLeftWritingDirection"];
}

}  // namespace ios
}  // namespace base
