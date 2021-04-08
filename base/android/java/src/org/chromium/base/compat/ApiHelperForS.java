// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.content.ClipDescription;
import android.content.pm.PackageManager;
import android.os.Process;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnS;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Utility class to use new APIs that were added in S (API level 31). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnS
@TargetApi(BuildInfo.ANDROID_S_API_SDK_INT)
public final class ApiHelperForS {
    private static final String TAG = "ApiHelperForS";

    private ApiHelperForS() {}

    /**
     * See {@link ClipDescription#isStyleText()}.
     */
    public static boolean isStyleText(ClipDescription clipDescription) {
        try {
            Method isStyledTextMethod = ClipDescription.class.getDeclaredMethod("isStyledText");
            return (boolean) isStyledTextMethod.invoke(clipDescription);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException e) {
            Log.e(TAG, "Failed to invoke ClipDescription#isStyledText() ", e);
            return false;
        }
    }

    public static boolean hasBluetoothConnectPermission() {
        // TODO(b/183501112): Replace the permission string with the actual Manfifest constant once
        // Chrome starts compiling against the S SDK.
        return ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                       "android.permission.BLUETOOTH_CONNECT", Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }
}
