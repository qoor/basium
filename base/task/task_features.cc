// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/feature_list.h"

namespace base {

const base::Feature kMergeBlockingNonBlockingPools = {
    "MergeBlockingNonBlockingPools", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace base