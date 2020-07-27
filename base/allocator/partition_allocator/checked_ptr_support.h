// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_

#define ENABLE_TAG_FOR_CHECKED_PTR2 0
#define ENABLE_TAG_FOR_MTE_CHECKED_PTR 0
#define ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR 0

// This is a sub-variant of ENABLE_TAG_FOR_MTE_CHECKED_PTR
#define MTE_CHECKED_PTR_SET_TAG_AT_FREE 1

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
