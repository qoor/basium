// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_loader_hooks.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/android/library_loader/library_prefetcher.h"
#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/android/sys_utils.h"
#include "base/at_exit.h"
#include "base/base_jni_headers/LibraryLoader_jni.h"
#include "base/base_switches.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
#include "base/android/orderfile/orderfile_instrumentation.h"
#endif

namespace base {
namespace android {

namespace {

base::AtExitManager* g_at_exit_manager = NULL;
const char* g_library_version_number = "";
LibraryLoadedHook* g_registration_callback = NULL;
NativeInitializationHook* g_native_initialization_hook = NULL;

// Indicates whether g_library_preloader_renderer_histogram_code is valid.
bool g_library_preloader_renderer_histogram_code_registered = false;

// The return value of NativeLibraryPreloader.loadLibrary() in child processes,
// it is initialized to the invalid value which shouldn't showup in UMA report.
int g_library_preloader_renderer_histogram_code = -1;

// The amount of time, in milliseconds, that it took to load the shared
// libraries in the renderer. Set in
// JNI_LibraryLoader_RegisterChromiumAndroidLinkerRendererHistogram.
long g_renderer_library_load_time_ms = 0;

void RecordChromiumAndroidLinkerRendererHistogram() {
  // Record how long it took to load the shared libraries.
  UMA_HISTOGRAM_TIMES("ChromiumAndroidLinker.RendererLoadTime",
      base::TimeDelta::FromMilliseconds(g_renderer_library_load_time_ms));
}

void RecordLibraryPreloaderRendereHistogram() {
  if (g_library_preloader_renderer_histogram_code_registered) {
    UmaHistogramSparse("Android.NativeLibraryPreloader.Result.Renderer",
                       g_library_preloader_renderer_histogram_code);
  }
}

}  // namespace

bool IsUsingOrderfileOptimization() {
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  return SysUtils::IsLowEndDeviceFromJni();
#else  //  !SUPPORTS_CODE_ORDERING
  return false;
#endif
}

static void JNI_LibraryLoader_RegisterChromiumAndroidLinkerRendererHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong library_load_time_ms) {
  g_renderer_library_load_time_ms = library_load_time_ms;
}

static void JNI_LibraryLoader_RecordChromiumAndroidLinkerBrowserHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong library_load_time_ms) {
  // Record how long it took to load the shared libraries.
  UMA_HISTOGRAM_TIMES("ChromiumAndroidLinker.BrowserLoadTime",
                      base::TimeDelta::FromMilliseconds(library_load_time_ms));
}

static void JNI_LibraryLoader_RecordLibraryPreloaderBrowserHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint status) {
  UmaHistogramSparse("Android.NativeLibraryPreloader.Result.Browser", status);
}

static void JNI_LibraryLoader_RegisterLibraryPreloaderRendererHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint status) {
  g_library_preloader_renderer_histogram_code = status;
  g_library_preloader_renderer_histogram_code_registered = true;
}

void SetNativeInitializationHook(
    NativeInitializationHook native_initialization_hook) {
  g_native_initialization_hook = native_initialization_hook;
}

void RecordLibraryLoaderRendererHistograms() {
  RecordChromiumAndroidLinkerRendererHistogram();
  RecordLibraryPreloaderRendereHistogram();
}

void SetLibraryLoadedHook(LibraryLoadedHook* func) {
  g_registration_callback = func;
}

static jboolean JNI_LibraryLoader_LibraryLoaded(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint library_process_type) {
#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  orderfile::StartDelayedDump();
#endif

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          "log-native-library-residency")) {
    NativeLibraryPrefetcher::MadviseForResidencyCollection();
  } else if (IsUsingOrderfileOptimization()) {
    NativeLibraryPrefetcher::MadviseForOrderfile();
  }
#endif

  if (g_native_initialization_hook &&
      !g_native_initialization_hook(
          static_cast<LibraryProcessType>(library_process_type)))
    return false;
  if (g_registration_callback &&
      !g_registration_callback(
          env, nullptr,
          static_cast<LibraryProcessType>(library_process_type))) {
    return false;
  }
  return true;
}

void LibraryLoaderExitHook() {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

void SetVersionNumber(const char* version_number) {
  g_library_version_number = strdup(version_number);
}

ScopedJavaLocalRef<jstring> JNI_LibraryLoader_GetVersionNumber(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return ConvertUTF8ToJavaString(env, g_library_version_number);
}

void InitAtExitManager() {
  g_at_exit_manager = new base::AtExitManager();
}

}  // namespace android
}  // namespace base
