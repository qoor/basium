// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import org.chromium.base.library_loader.LibraryLoader;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/**
 * Utility classes for recording UMA metrics before the native library
 * may have been loaded.  Metrics are cached until the library is known
 * to be loaded, then committed to the MetricsService all at once.
 *
 * NOTE: Currently supports recording metrics only in Browser/Webview/Renderer processes.
 */
public class CachedMetrics {
    /**
     * Base class for cached metric objects. Subclasses are expected to call
     * addToCache() when some metric state gets recorded that requires a later
     * commit operation when the native library is loaded.
     */
    private abstract static class CachedMetric {
        @GuardedBy("sMetrics")
        private static final List<CachedMetric> sMetrics = new ArrayList<CachedMetric>();

        /**
         * Randomly choose whether to always immediately invoke {@link RecordHistogram}. This value
         * is recorded using a synthetic field trial.
         */
        protected static final boolean HISTOGRAMS_BYPASS_CACHE = Math.random() < 0.5;

        protected final String mName;
        protected boolean mCached;

        /**
         * @param name Name of the metric to record.
         */
        protected CachedMetric(String name) {
            mName = name;
        }

        /**
         * Adds this object to the sMetrics cache, if it hasn't been added already.
         * Must be called while holding the synchronized(sMetrics) lock.
         * Note: The synchronization is not done inside this function because subclasses
         * need to increment their held values under lock to ensure thread-safety.
         */
        @GuardedBy("sMetrics")
        protected final void addToCache() {
            assert Thread.holdsLock(sMetrics);

            if (mCached) return;
            sMetrics.add(this);
            mCached = true;
        }

        /**
         * Commits the metric. Expects the native library to be loaded.
         * Must be called while holding the synchronized(sMetrics) lock.
         */
        @GuardedBy("sMetrics")
        protected abstract void commitAndClear();

        /**
         * Returns {@code true} if histogram samples should be immediately recorded.
         * <p>
         * Handles two cases:
         * <ul>
         *     <li>Cache is not needed, because {@link LibraryLoader} is already initialized.
         *     <li>Cache is disabled to validate caching present in {@link RecordHistogram}.
         * </ul>
         */
        protected static boolean shouldHistogramBypassCache() {
            return HISTOGRAMS_BYPASS_CACHE || LibraryLoader.getInstance().isInitialized();
        }
    }

    /**
     * Caches an action that will be recorded after native side is loaded.
     */
    public static class ActionEvent extends CachedMetric {
        @GuardedBy("CachedMetric.sMetrics")
        private int mCount;

        public ActionEvent(String actionName) {
            super(actionName);
        }

        public void record() {
            synchronized (CachedMetric.sMetrics) {
                if (LibraryLoader.getInstance().isInitialized()) {
                    recordWithNative();
                } else {
                    mCount++;
                    addToCache();
                }
            }
        }

        private void recordWithNative() {
            RecordUserAction.record(mName);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            while (mCount > 0) {
                recordWithNative();
                mCount--;
            }
        }
    }

    /**
     * Caches a set of integer histogram samples.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class SparseHistogramSample extends CachedMetric {
        @GuardedBy("CachedMetric.sMetrics")
        private final List<Integer> mSamples = new ArrayList<Integer>();

        public SparseHistogramSample(String histogramName) {
            super(histogramName);
        }

        public void record(int sample) {
            synchronized (CachedMetric.sMetrics) {
                if (shouldHistogramBypassCache()) {
                    recordWithNative(sample);
                } else {
                    mSamples.add(sample);
                    addToCache();
                }
            }
        }

        private void recordWithNative(int sample) {
            RecordHistogram.recordSparseHistogram(mName, sample);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            for (Integer sample : mSamples) {
                recordWithNative(sample);
            }
            mSamples.clear();
        }
    }

    /**
     * Caches a set of enumerated histogram samples.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class EnumeratedHistogramSample extends CachedMetric {
        private final List<Integer> mSamples = new ArrayList<Integer>();
        private final int mMaxValue;

        public EnumeratedHistogramSample(String histogramName, int maxValue) {
            super(histogramName);
            mMaxValue = maxValue;
        }

        public void record(int sample) {
            synchronized (CachedMetric.sMetrics) {
                if (shouldHistogramBypassCache()) {
                    recordWithNative(sample);
                } else {
                    mSamples.add(sample);
                    addToCache();
                }
            }
        }

        private void recordWithNative(int sample) {
            RecordHistogram.recordEnumeratedHistogram(mName, sample, mMaxValue);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            for (Integer sample : mSamples) {
                recordWithNative(sample);
            }
            mSamples.clear();
        }
    }

    /**
     * Caches a set of times histogram samples.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class TimesHistogramSample extends CachedMetric {
        @GuardedBy("CachedMetric.sMetrics")
        private final List<Long> mSamples = new ArrayList<Long>();

        public TimesHistogramSample(String histogramName) {
            super(histogramName);
        }

        public void record(long sample) {
            synchronized (CachedMetric.sMetrics) {
                if (shouldHistogramBypassCache()) {
                    recordWithNative(sample);
                } else {
                    mSamples.add(sample);
                    addToCache();
                }
            }
        }

        protected void recordWithNative(long sample) {
            RecordHistogram.recordTimesHistogram(mName, sample);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            for (Long sample : mSamples) {
                recordWithNative(sample);
            }
            mSamples.clear();
        }
    }

    /**
     * Caches a set of times histogram samples, calls
     * {@link RecordHistogram#recordMediumTimesHistogram(String, long)}.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class MediumTimesHistogramSample extends TimesHistogramSample {
        public MediumTimesHistogramSample(String histogramName) {
            super(histogramName);
        }

        @Override
        protected void recordWithNative(long sample) {
            RecordHistogram.recordMediumTimesHistogram(mName, sample);
        }
    }

    /**
     * Caches a set of boolean histogram samples.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class BooleanHistogramSample extends CachedMetric {
        @GuardedBy("CachedMetric.sMetrics")
        private final List<Boolean> mSamples = new ArrayList<Boolean>();

        public BooleanHistogramSample(String histogramName) {
            super(histogramName);
        }

        public void record(boolean sample) {
            synchronized (CachedMetric.sMetrics) {
                if (shouldHistogramBypassCache()) {
                    recordWithNative(sample);
                } else {
                    mSamples.add(sample);
                    addToCache();
                }
            }
        }

        private void recordWithNative(boolean sample) {
            RecordHistogram.recordBooleanHistogram(mName, sample);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            for (Boolean sample : mSamples) {
                recordWithNative(sample);
            }
            mSamples.clear();
        }
    }

    /**
     * Caches a set of custom count histogram samples.
     * Corresponds to UMA_HISTOGRAM_CUSTOM_COUNTS C++ macro.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class CustomCountHistogramSample extends CachedMetric {
        @GuardedBy("CachedMetric.sMetrics")
        private final List<Integer> mSamples = new ArrayList<Integer>();
        protected final int mMin;
        protected final int mMax;
        protected final int mNumBuckets;

        public CustomCountHistogramSample(String histogramName, int min, int max, int numBuckets) {
            super(histogramName);
            mMin = min;
            mMax = max;
            mNumBuckets = numBuckets;
        }

        public void record(int sample) {
            synchronized (CachedMetric.sMetrics) {
                if (shouldHistogramBypassCache()) {
                    recordWithNative(sample);
                } else {
                    mSamples.add(sample);
                    addToCache();
                }
            }
        }

        protected void recordWithNative(int sample) {
            RecordHistogram.recordCustomCountHistogram(mName, sample, mMin, mMax, mNumBuckets);
        }

        @Override
        @GuardedBy("CachedMetric.sMetrics")
        protected void commitAndClear() {
            for (Integer sample : mSamples) {
                recordWithNative(sample);
            }
            mSamples.clear();
        }
    }

    /**
     * Caches a set of count histogram samples in range [1, 100).
     * Corresponds to UMA_HISTOGRAM_COUNTS_100 C++ macro.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class Count100HistogramSample extends CustomCountHistogramSample {
        public Count100HistogramSample(String histogramName) {
            super(histogramName, 1, 100, 50);
        }
    }

    /**
     * Caches a set of count histogram samples in range [1, 1000).
     * Corresponds to UMA_HISTOGRAM_COUNTS_1000 C++ macro.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class Count1000HistogramSample extends CustomCountHistogramSample {
        public Count1000HistogramSample(String histogramName) {
            super(histogramName, 1, 1000, 50);
        }
    }

    /**
     * Caches a set of count histogram samples in range [1, 1000000).
     * Corresponds to UMA_HISTOGRAM_COUNTS_1M C++ macro.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class Count1MHistogramSample extends CustomCountHistogramSample {
        public Count1MHistogramSample(String histogramName) {
            super(histogramName, 1, 1000000, 50);
        }
    }

    /**
     * Caches a set of linear count histogram samples.
     *
     * @deprecated Use {@link RecordHistogram} instead.
     */
    @Deprecated
    public static class LinearCountHistogramSample extends CustomCountHistogramSample {
        public LinearCountHistogramSample(String histogramName, int min, int max, int numBuckets) {
            super(histogramName, min, max, numBuckets);
        }

        @Override
        protected void recordWithNative(int sample) {
            RecordHistogram.recordLinearCountHistogram(mName, sample, mMin, mMax, mNumBuckets);
        }
    }

    /**
     * Calls out to native code to commit any cached histograms and events.
     * Should be called once the native library has been initialized.
     */
    public static void commitCachedMetrics() {
        synchronized (CachedMetric.sMetrics) {
            for (CachedMetric metric : CachedMetric.sMetrics) {
                metric.commitAndClear();
            }
        }
    }

    /** Returns true if histograms are immediately recorded with {@link UmaRecorder}. */
    public static boolean histogramsBypassCache() {
        return CachedMetric.HISTOGRAMS_BYPASS_CACHE;
    }
}
