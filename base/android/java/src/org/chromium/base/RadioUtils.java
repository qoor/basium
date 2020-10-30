// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.Manifest;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Process;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes radio related information about the current device.
 */
@JNINamespace("base::android")
public class RadioUtils {
    // Cached value indicating if app has ACCESS_NETWORK_STATE permission.
    private static Boolean sHaveAccessNetworkState;
    // Cached value indicating if app has ACCESS_WIFI_STATE permission.
    private static Boolean sHaveAccessWifiState;

    private RadioUtils() {}

    /**
     * Return whether the current SDK supports necessary functions and the app
     * has necessary permissions.
     * @return True or false.
     */
    @CalledByNative
    private static boolean isSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && haveAccessNetworkState()
                && haveAccessWifiState();
    }

    private static boolean haveAccessNetworkState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessNetworkState == null) {
            sHaveAccessNetworkState =
                    ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                            Manifest.permission.ACCESS_NETWORK_STATE, Process.myPid(),
                            Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }
        return sHaveAccessNetworkState;
    }

    private static boolean haveAccessWifiState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessWifiState == null) {
            sHaveAccessWifiState =
                    ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                            Manifest.permission.ACCESS_WIFI_STATE, Process.myPid(), Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }
        return sHaveAccessWifiState;
    }

    /**
     * Return whether the device is currently connected to a wifi network.
     * @return True or false.
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static boolean isWifiConnected() {
        assert isSupported();
        ConnectivityManager connectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        Network network = connectivityManager.getActiveNetwork();
        if (network == null) return false;
        NetworkCapabilities networkCapabilities =
                connectivityManager.getNetworkCapabilities(network);
        if (networkCapabilities == null) return false;
        return networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI);
    }

    /**
     * Return current cell signal level.
     * @return Signal level from 0 (no signal) to 4 (good signal).
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static int getCellSignalLevel() {
        assert isSupported();
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);
        SignalStrength signalStrength = telephonyManager.getSignalStrength();
        if (signalStrength == null) return -1;
        return signalStrength.getLevel();
    }
}
