// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.util.Log;
import android.util.Printer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
/**
 * Java mirror of Chrome trace event API. See base/trace_event/trace_event.h.
 *
 * To get scoped trace events, use the "try with resource" construct, for instance:
 * <pre>{@code
 * try (TraceEvent e = TraceEvent.scoped("MyTraceEvent")) {
 *   // code.
 * }
 * }</pre>
 *
 * The event name of the trace events must be a string literal or a |static final String| class
 * member. Otherwise NoDynamicStringsInTraceEventCheck error will be thrown.
 *
 * It is OK to use tracing before the native library has loaded, in a slightly restricted fashion.
 * @see EarlyTraceEvent for details.
 */
@JNINamespace("base::android")
@MainDex
public class TraceEvent implements AutoCloseable {
    private static volatile boolean sEnabled;
    private static volatile boolean sATraceEnabled; // True when taking an Android systrace.

    private static class BasicLooperMonitor implements Printer {
        private static final String LOOPER_TASK_PREFIX = "Looper.dispatch: ";
        private static final int SHORTEST_LOG_PREFIX_LENGTH = "<<<<< Finished to ".length();
        private String mCurrentTarget;

        @Override
        public void println(final String line) {
            if (line.startsWith(">")) {
                beginHandling(line);
            } else {
                assert line.startsWith("<");
                endHandling(line);
            }
        }

        void beginHandling(final String line) {
            // May return an out-of-date value. this is not an issue as EarlyTraceEvent#begin()
            // will filter the event in this case.
            boolean earlyTracingActive = EarlyTraceEvent.enabled();
            if (sEnabled || earlyTracingActive) {
                mCurrentTarget = getTraceEventName(line);
                if (sEnabled) {
                    TraceEventJni.get().beginToplevel(mCurrentTarget);
                } else {
                    EarlyTraceEvent.begin(mCurrentTarget, true /*isToplevel*/);
                }
            }
        }

        void endHandling(final String line) {
            boolean earlyTracingActive = EarlyTraceEvent.enabled();
            if ((sEnabled || earlyTracingActive) && mCurrentTarget != null) {
                if (sEnabled) {
                    TraceEventJni.get().endToplevel(mCurrentTarget);
                } else {
                    EarlyTraceEvent.end(mCurrentTarget, true /*isToplevel*/);
                }
            }
            mCurrentTarget = null;
        }

        private static String getTraceEventName(String line) {
            return LOOPER_TASK_PREFIX + getTarget(line) + "(" + getTargetName(line) + ")";
        }

        /**
         * Android Looper formats |logLine| as
         *
         * ">>>>> Dispatching to (TARGET) {HASH_CODE} TARGET_NAME: WHAT"
         *
         * and
         *
         * "<<<<< Finished to (TARGET) {HASH_CODE} TARGET_NAME".
         *
         * This has been the case since at least 2009 (Donut). This function extracts the
         * TARGET part of the message.
         */
        private static String getTarget(String logLine) {
            int start = logLine.indexOf('(', SHORTEST_LOG_PREFIX_LENGTH);
            int end = start == -1 ? -1 : logLine.indexOf(')', start);
            return end != -1 ? logLine.substring(start + 1, end) : "";
        }

        // Extracts the TARGET_NAME part of the log message (see above).
        private static String getTargetName(String logLine) {
            int start = logLine.indexOf('}', SHORTEST_LOG_PREFIX_LENGTH);
            int end = start == -1 ? -1 : logLine.indexOf(':', start);
            if (end == -1) {
                end = logLine.length();
            }
            return start != -1 ? logLine.substring(start + 2, end) : "";
        }
    }

    /**
     * A class that records, traces and logs statistics about the UI thead's Looper.
     * The output of this class can be used in a number of interesting ways:
     * <p>
     * <ol><li>
     * When using chrometrace, there will be a near-continuous line of
     * measurements showing both event dispatches as well as idles;
     * </li><li>
     * Logging messages are output for events that run too long on the
     * event dispatcher, making it easy to identify problematic areas;
     * </li><li>
     * Statistics are output whenever there is an idle after a non-trivial
     * amount of activity, allowing information to be gathered about task
     * density and execution cadence on the Looper;
     * </li></ol>
     * <p>
     * The class attaches itself as an idle handler to the main Looper, and
     * monitors the execution of events and idle notifications. Task counters
     * accumulate between idle notifications and get reset when a new idle
     * notification is received.
     */
    private static final class IdleTracingLooperMonitor extends BasicLooperMonitor
            implements MessageQueue.IdleHandler {
        // Tags for dumping to logcat or TraceEvent
        private static final String TAG = "TraceEvent_LooperMonitor";
        private static final String IDLE_EVENT_NAME = "Looper.queueIdle";

        // Calculation constants
        private static final long FRAME_DURATION_MILLIS = 1000L / 60L; // 60 FPS
        // A reasonable threshold for defining a Looper event as "long running"
        private static final long MIN_INTERESTING_DURATION_MILLIS =
                FRAME_DURATION_MILLIS;
        // A reasonable threshold for a "burst" of tasks on the Looper
        private static final long MIN_INTERESTING_BURST_DURATION_MILLIS =
                MIN_INTERESTING_DURATION_MILLIS * 3;

        // Stats tracking
        private long mLastIdleStartedAt;
        private long mLastWorkStartedAt;
        private int mNumTasksSeen;
        private int mNumIdlesSeen;
        private int mNumTasksSinceLastIdle;

        // State
        private boolean mIdleMonitorAttached;

        // Called from within the begin/end methods only.
        // This method can only execute on the looper thread, because that is
        // the only thread that is permitted to call Looper.myqueue().
        private final void syncIdleMonitoring() {
            if (sEnabled && !mIdleMonitorAttached) {
                // approximate start time for computational purposes
                mLastIdleStartedAt = SystemClock.elapsedRealtime();
                Looper.myQueue().addIdleHandler(this);
                mIdleMonitorAttached = true;
                Log.v(TAG, "attached idle handler");
            } else if (mIdleMonitorAttached && !sEnabled) {
                Looper.myQueue().removeIdleHandler(this);
                mIdleMonitorAttached = false;
                Log.v(TAG, "detached idle handler");
            }
        }

        @Override
        final void beginHandling(final String line) {
            // Close-out any prior 'idle' period before starting new task.
            if (mNumTasksSinceLastIdle == 0) {
                TraceEvent.end(IDLE_EVENT_NAME);
            }
            mLastWorkStartedAt = SystemClock.elapsedRealtime();
            syncIdleMonitoring();
            super.beginHandling(line);
        }

        @Override
        final void endHandling(final String line) {
            final long elapsed = SystemClock.elapsedRealtime()
                    - mLastWorkStartedAt;
            if (elapsed > MIN_INTERESTING_DURATION_MILLIS) {
                traceAndLog(Log.WARN, "observed a task that took "
                        + elapsed + "ms: " + line);
            }
            super.endHandling(line);
            syncIdleMonitoring();
            mNumTasksSeen++;
            mNumTasksSinceLastIdle++;
        }

        private static void traceAndLog(int level, String message) {
            TraceEvent.instant("TraceEvent.LooperMonitor:IdleStats", message);
            Log.println(level, TAG, message);
        }

        @Override
        public final boolean queueIdle() {
            final long now =  SystemClock.elapsedRealtime();
            if (mLastIdleStartedAt == 0) mLastIdleStartedAt = now;
            final long elapsed = now - mLastIdleStartedAt;
            mNumIdlesSeen++;
            TraceEvent.begin(IDLE_EVENT_NAME, mNumTasksSinceLastIdle + " tasks since last idle.");
            if (elapsed > MIN_INTERESTING_BURST_DURATION_MILLIS) {
                // Dump stats
                String statsString = mNumTasksSeen + " tasks and "
                        + mNumIdlesSeen + " idles processed so far, "
                        + mNumTasksSinceLastIdle + " tasks bursted and "
                        + elapsed + "ms elapsed since last idle";
                traceAndLog(Log.DEBUG, statsString);
            }
            mLastIdleStartedAt = now;
            mNumTasksSinceLastIdle = 0;
            return true; // stay installed
        }
    }

    // Holder for monitor avoids unnecessary construction on non-debug runs
    private static final class LooperMonitorHolder {
        private static final BasicLooperMonitor sInstance =
                CommandLine.getInstance().hasSwitch(BaseSwitches.ENABLE_IDLE_TRACING)
                ? new IdleTracingLooperMonitor() : new BasicLooperMonitor();
    }

    private final String mName;

    /**
     * Constructor used to support the "try with resource" construct.
     */
    private TraceEvent(String name, String arg) {
        mName = name;
        begin(name, arg);
    }

    @Override
    public void close() {
        end(mName);
    }

    /**
     * Factory used to support the "try with resource" construct.
     *
     * Note that if tracing is not enabled, this will not result in allocating an object.
     *
     * @param name Trace event name.
     * @param arg The arguments of the event.
     * @return a TraceEvent, or null if tracing is not enabled.
     */
    public static TraceEvent scoped(String name, String arg) {
        if (!(EarlyTraceEvent.enabled() || enabled())) return null;
        return new TraceEvent(name, arg);
    }

    /**
     * Similar to {@link #scoped(String, String arg)}, but uses null for |arg|.
     */
    public static TraceEvent scoped(String name) {
        return scoped(name, null);
    }

    /**
     * Register an enabled observer, such that java traces are always enabled with native.
     */
    public static void registerNativeEnabledObserver() {
        TraceEventJni.get().registerEnabledObserver();
    }

    /**
     * Notification from native that tracing is enabled/disabled.
     */
    @CalledByNative
    public static void setEnabled(boolean enabled) {
        if (enabled) EarlyTraceEvent.disable();
        // Only disable logging if Chromium enabled it originally, so as to not disrupt logging done
        // by other applications
        if (sEnabled != enabled) {
            sEnabled = enabled;
            // Android M+ systrace logs this on its own. Only log it if not writing to Android
            // systrace.
            if (sATraceEnabled) return;
            ThreadUtils.getUiThreadLooper().setMessageLogging(
                    enabled ? LooperMonitorHolder.sInstance : null);
        }
    }

    /**
     * May enable early tracing depending on the environment.
     *
     * Must be called after the command-line has been read.
     */
    public static void maybeEnableEarlyTracing() {
        EarlyTraceEvent.maybeEnable();
        if (EarlyTraceEvent.enabled()) {
            ThreadUtils.getUiThreadLooper().setMessageLogging(LooperMonitorHolder.sInstance);
        }
    }

    /**
     * Enables or disabled Android systrace path of Chrome tracing. If enabled, all Chrome
     * traces will be also output to Android systrace. Because of the overhead of Android
     * systrace, this is for WebView only.
     */
    public static void setATraceEnabled(boolean enabled) {
        if (sATraceEnabled == enabled) return;
        sATraceEnabled = enabled;
        if (enabled) {
            // Calls TraceEvent.setEnabled(true) via
            // TraceLog::EnabledStateObserver::OnTraceLogEnabled
            TraceEventJni.get().startATrace();
        } else {
            // Calls TraceEvent.setEnabled(false) via
            // TraceLog::EnabledStateObserver::OnTraceLogDisabled
            TraceEventJni.get().stopATrace();
        }
    }

    /**
     * @return True if tracing is enabled, false otherwise.
     * It is safe to call trace methods without checking if TraceEvent
     * is enabled.
     */
    public static boolean enabled() {
        return sEnabled;
    }

    /**
     * Triggers the 'instant' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void instant(String name) {
        if (sEnabled) TraceEventJni.get().instant(name, null);
    }

    /**
     * Triggers the 'instant' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void instant(String name, String arg) {
        if (sEnabled) TraceEventJni.get().instant(name, arg);
    }

    /**
     * Triggers the 'start' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     */
    public static void startAsync(String name, long id) {
        EarlyTraceEvent.startAsync(name, id);
        if (sEnabled) TraceEventJni.get().startAsync(name, id);
    }

    /**
     * Triggers the 'finish' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     */
    public static void finishAsync(String name, long id) {
        EarlyTraceEvent.finishAsync(name, id);
        if (sEnabled) TraceEventJni.get().finishAsync(name, id);
    }

    /**
     * Triggers the 'begin' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void begin(String name) {
        begin(name, null);
    }

    /**
     * Triggers the 'begin' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void begin(String name, String arg) {
        EarlyTraceEvent.begin(name, false /*isToplevel*/);
        if (sEnabled) TraceEventJni.get().begin(name, arg);
    }

    /**
     * Triggers the 'end' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void end(String name) {
        end(name, null);
    }

    /**
     * Triggers the 'end' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void end(String name, String arg) {
        EarlyTraceEvent.end(name, false /*isToplevel*/);
        if (sEnabled) TraceEventJni.get().end(name, arg);
    }

    @NativeMethods
    interface Natives {
        void registerEnabledObserver();
        void startATrace();
        void stopATrace();
        void instant(String name, String arg);
        void begin(String name, String arg);
        void end(String name, String arg);
        void beginToplevel(String target);
        void endToplevel(String target);
        void startAsync(String name, long id);
        void finishAsync(String name, long id);
    }
}
