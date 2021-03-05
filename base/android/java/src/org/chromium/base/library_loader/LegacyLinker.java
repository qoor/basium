// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.base.Log;
import org.chromium.base.annotations.JniIgnoreNatives;

import javax.annotation.concurrent.GuardedBy;

/**
 * Provides a concrete implementation of the Chromium Linker.
 *
 * This Linker implementation uses the crazy linker to map and then run Chrome for Android.
 *
 * For more on the operations performed by the Linker, see {@link Linker}.
 */
@JniIgnoreNatives
class LegacyLinker extends Linker {
    // Log tag for this class.
    private static final String TAG = "LegacyLinker";

    LegacyLinker() {}

    @Override
    void setApkFilePath(String path) {
        synchronized (mLock) {
            ensureInitializedLocked();
            nativeAddZipArchivePath(path);
        }
    }

    @Override
    @GuardedBy("mLock")
    protected void loadLibraryImplLocked(
            String library, long loadAddress, @RelroSharingMode int relroMode) {
        assert mState == State.INITIALIZED; // Only one successful call.

        String libFilePath = System.mapLibraryName(library);
        LibInfo libInfo = new LibInfo();
        if (!nativeLoadLibrary(libFilePath, loadAddress, libInfo)) {
            String errorMessage = "Unable to load library: " + libFilePath;
            Log.e(TAG, errorMessage);
            throw new UnsatisfiedLinkError(errorMessage);
        }
        libInfo.mLibFilePath = libFilePath;

        if (relroMode == RelroSharingMode.PRODUCE || relroMode == RelroSharingMode.NO_SHARING) {
            if (!nativeCreateSharedRelro(libFilePath, loadAddress, libInfo)) {
                Log.w(TAG, "Could not create shared RELRO for %s at %x", libFilePath, loadAddress);
                // Next state is still to provide relro (even though we don't have any), as child
                // processes would wait for them.
                libInfo.mRelroFd = -1;
            } else {
                if (DEBUG) {
                    Log.i(TAG, "Created shared RELRO for %s at %x: %s", libFilePath, loadAddress,
                            libInfo.toString());
                }
            }
            mLibInfo = libInfo;
            useSharedRelrosLocked(mLibInfo);
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            assert relroMode == RelroSharingMode.CONSUME;
            waitForSharedRelrosLocked();
            assert libFilePath.equals(mLibInfo.mLibFilePath);
            useSharedRelrosLocked(mLibInfo);
            mLibInfo.close();
            mLibInfo = null;
            mState = State.DONE;
        }
    }

    /**
     * Use the shared RELRO section from a Bundle received form another process.
     *
     * @param info Object containing the RELRO FD.
     */
    private static void useSharedRelrosLocked(LibInfo info) {
        String libFilePath = info.mLibFilePath;
        if (!nativeUseSharedRelro(libFilePath, info)) {
            Log.w(TAG, "Could not use shared RELRO section for %s", libFilePath);
        } else {
            if (DEBUG) Log.i(TAG, "Using shared RELRO section for %s", libFilePath);
        }
    }

    /**
     * Native method used to load a library.
     *
     * @param library Platform specific library name (e.g. libfoo.so)
     * @param loadAddress Explicit load address, or 0 for randomized one.
     * @param libInfo If not null, the mLoadAddress and mLoadSize fields
     * of this LibInfo instance will set on success.
     * @return true for success, false otherwise.
     */
    private static native boolean nativeLoadLibrary(
            String library, long loadAddress, LibInfo libInfo);

    /**
     * Native method used to add a zip archive or APK to the search path
     * for native libraries. Allows loading directly from it.
     *
     * @param zipFilePath Path of the zip file containing the libraries.
     * @return true for success, false otherwise.
     */
    private static native boolean nativeAddZipArchivePath(String zipFilePath);

    /**
     * Native method used to create a shared RELRO section.
     * If the library was already loaded at the same address using
     * nativeLoadLibrary(), this creates the RELRO for it. Otherwise,
     * this loads a new temporary library at the specified address,
     * creates and extracts the RELRO section from it, then unloads it.
     *
     * @param library Library name.
     * @param loadAddress load address, which can be different from the one
     * used to load the library in the current process!
     * @param libInfo libInfo instance. On success, the mRelroStart, mRelroSize
     * and mRelroFd will be set.
     * @return true on success, false otherwise.
     */
    private static native boolean nativeCreateSharedRelro(
            String library, long loadAddress, LibInfo libInfo);

    /**
     * Native method used to use a shared RELRO section.
     *
     * @param library Library name.
     * @param libInfo A LibInfo instance containing valid RELRO information
     * @return true on success.
     */
    private static native boolean nativeUseSharedRelro(String library, LibInfo libInfo);
}
