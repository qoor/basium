// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_registry_simple.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

PrefRegistrySimple::PrefRegistrySimple() {
}

PrefRegistrySimple::~PrefRegistrySimple() {
}

void PrefRegistrySimple::RegisterBooleanPref(const std::string& path,
                                             bool default_value) {
  RegisterPreference(path, new base::FundamentalValue(default_value));
}

void PrefRegistrySimple::RegisterIntegerPref(const std::string& path,
                                             int default_value) {
  RegisterPreference(path, new base::FundamentalValue(default_value));
}

void PrefRegistrySimple::RegisterDoublePref(const std::string& path,
                                            double default_value) {
  RegisterPreference(path, new base::FundamentalValue(default_value));
}

void PrefRegistrySimple::RegisterStringPref(const std::string& path,
                                            const std::string& default_value) {
  RegisterPreference(path, new base::StringValue(default_value));
}

void PrefRegistrySimple::RegisterFilePathPref(
    const std::string& path,
    const base::FilePath& default_value) {
  RegisterPreference(path, new base::StringValue(default_value.value()));
}

void PrefRegistrySimple::RegisterListPref(const std::string& path) {
  RegisterPreference(path, new base::ListValue());
}

void PrefRegistrySimple::RegisterListPref(const std::string& path,
                                          base::ListValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefRegistrySimple::RegisterDictionaryPref(const std::string& path) {
  RegisterPreference(path, new base::DictionaryValue());
}

void PrefRegistrySimple::RegisterDictionaryPref(
    const std::string& path,
    base::DictionaryValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefRegistrySimple::RegisterInt64Pref(const std::string& path,
                                           int64 default_value) {
  RegisterPreference(
      path, new base::StringValue(base::Int64ToString(default_value)));
}
