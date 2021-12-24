// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rs_glue/values_glue.h"
#include <stddef.h>
#include <sstream>

namespace base {
namespace rs_glue {

// This file has functions which are called from Rust code to populate
// bits of a base::Value. The functions exist because Rust C++ FFI
// is not yet quite good enough to operate on a base::Value directly
// without these intermediate layer. With future inprovements in interop,
// they may disappear.

std::unique_ptr<ValueSlot> NewValueSlot() {
  return std::make_unique<ValueSlot>();
}

void ValueSetNoneKey(base::Value& v, rust::Str key) {
  v.SetKey(base::RustStrToStringPiece(key),
           base::Value(base::Value::Type::NONE));
}

void ValueSetBoolKey(base::Value& v, rust::Str key, bool value) {
  v.SetBoolKey(base::RustStrToStringPiece(key), value);
}

void ValueSetIntegerKey(base::Value& v, rust::Str key, int value) {
  v.SetIntKey(base::RustStrToStringPiece(key), value);
}

void ValueSetDoubleKey(base::Value& v, rust::Str key, double value) {
  v.SetDoubleKey(base::RustStrToStringPiece(key), value);
}

void ValueSetStringKey(base::Value& v, rust::Str key, rust::Str value) {
  v.SetStringKey(base::RustStrToStringPiece(key),
                 base::RustStrToStringPiece(value));
}

base::Value& ValueSetDictKey(base::Value& v, rust::Str key) {
  return *v.SetKey(base::RustStrToStringPiece(key),
                   base::Value(base::Value::Type::DICTIONARY));
}

base::Value& ValueSetListKey(base::Value& v, rust::Str key) {
  return *v.SetKey(base::RustStrToStringPiece(key),
                   base::Value(base::Value::Type::LIST));
}

template <typename T>
base::Value& DoSetVecItem(base::Value& v, size_t pos, T value) {
  if (v.GetList().size() < pos + 1) {
    auto list = std::move(v).TakeList();
    list.resize(pos + 1);
    v = base::Value(std::move(list));
  }
  auto list2 = v.GetList();
  list2[pos] = base::Value(std::move(value));
  return list2[pos];
}

void ValueSetNoneElement(base::Value& v, size_t pos) {
  DoSetVecItem(v, pos, base::Value::Type::NONE);
}

void ValueSetBoolElement(base::Value& v, size_t pos, bool value) {
  DoSetVecItem(v, pos, value);
}

void ValueSetIntegerElement(base::Value& v, size_t pos, int value) {
  DoSetVecItem(v, pos, value);
}

void ValueSetDoubleElement(base::Value& v, size_t pos, double value) {
  DoSetVecItem(v, pos, value);
}

void ValueSetStringElement(base::Value& v, size_t pos, rust::Str value) {
  DoSetVecItem(v, pos, base::RustStrToStringPiece(value));
}

base::Value& ValueSetDictElement(base::Value& v, size_t pos) {
  return DoSetVecItem(v, pos, base::Value::Type::DICTIONARY);
}

base::Value& ValueSetListElement(base::Value& v, size_t pos) {
  return DoSetVecItem(v, pos, base::Value::Type::LIST);
}

void ValueReserveSize(base::Value& v, size_t len) {
  DCHECK(v.is_list());
  base::Value::ListStorage l = std::move(v).TakeList();
  l.reserve(len);
  v = base::Value(std::move(l));
}

rust::String DumpValueSlot(const ValueSlot& v) {
  std::ostringstream os;
  if (v.has_value()) {
    os << *v;
  } else {
    os << "(empty)";
  }
  return rust::String(os.str());
}

void ConstructNoneValue(ValueSlot& v) {
  v.emplace(base::Value::Type::NONE);
}

void ConstructBoolValue(ValueSlot& v, bool value) {
  v.emplace(value);
}

void ConstructIntegerValue(ValueSlot& v, int value) {
  v.emplace(value);
}

void ConstructDoubleValue(ValueSlot& v, double value) {
  v.emplace(value);
}

void ConstructStringValue(ValueSlot& v, rust::Str value) {
  v.emplace(base::RustStrToStringPiece(value));
}

base::Value& ConstructDictValue(ValueSlot& v) {
  return v.emplace(base::Value::Type::DICTIONARY);
}

base::Value& ConstructListValue(ValueSlot& v) {
  return v.emplace(base::Value::Type::LIST);
}

}  // namespace rs_glue
}  // namespace base
