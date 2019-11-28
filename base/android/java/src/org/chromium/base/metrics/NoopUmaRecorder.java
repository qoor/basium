// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

/**
 * An empty implementation of {@link UmaRecorder} used by {@link
 * RecordHistogram#setDisabledForTests(boolean)}.
 */
/* package */ final class NoopUmaRecorder implements UmaRecorder {
    /* package */ NoopUmaRecorder() {}

    @Override
    public void recordBooleanHistogram(String name, boolean sample) {}

    @Override
    public void recordExponentialHistogram(
            String name, int sample, int min, int max, int numBuckets) {}

    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {}

    @Override
    public void recordSparseHistogram(String name, int sample) {}
}
