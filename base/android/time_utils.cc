// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/time_utils.h"

#include <stdint.h>

#include "base/time/time.h"
#include "jni/TimeUtils_jni.h"

namespace base {
namespace android {

static jlong GetTimeTicksNowUs(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  return (TimeTicks::Now() - TimeTicks()).InMicroseconds();
}

bool RegisterTimeUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
