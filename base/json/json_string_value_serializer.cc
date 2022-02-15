// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

using base::Value;

JSONStringValueSerializer::JSONStringValueSerializer(std::string* json_string)
    : json_string_(json_string),
      pretty_print_(false) {
}

JSONStringValueSerializer::~JSONStringValueSerializer() = default;

bool JSONStringValueSerializer::Serialize(const Value& root) {
  return SerializeInternal(root, false);
}

bool JSONStringValueSerializer::Serialize(const Value::Dict& root) {
  return SerializeInternal(root, false);
}

bool JSONStringValueSerializer::Serialize(const Value::List& root) {
  return SerializeInternal(root, false);
}

bool JSONStringValueSerializer::SerializeAndOmitBinaryValues(
    const Value& root) {
  return SerializeInternal(root, true);
}

bool JSONStringValueSerializer::SerializeAndOmitBinaryValues(
    const Value::Dict& root) {
  return SerializeInternal(root, true);
}

bool JSONStringValueSerializer::SerializeAndOmitBinaryValues(
    const Value::List& root) {
  return SerializeInternal(root, true);
}

bool JSONStringValueSerializer::SerializeInternal(const Value& root,
                                                  bool omit_binary_values) {
  if (!json_string_)
    return false;

  int options = 0;
  if (omit_binary_values)
    options |= base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES;
  if (pretty_print_)
    options |= base::JSONWriter::OPTIONS_PRETTY_PRINT;

  return base::JSONWriter::WriteWithOptions(root, options, json_string_);
}

bool JSONStringValueSerializer::SerializeInternal(const Value::Dict& root,
                                                  bool omit_binary_values) {
  if (!json_string_)
    return false;

  int options = 0;
  if (omit_binary_values)
    options |= base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES;
  if (pretty_print_)
    options |= base::JSONWriter::OPTIONS_PRETTY_PRINT;

  return base::JSONWriter::WriteWithOptions(root, options, json_string_);
}

bool JSONStringValueSerializer::SerializeInternal(const Value::List& root,
                                                  bool omit_binary_values) {
  if (!json_string_)
    return false;

  int options = 0;
  if (omit_binary_values)
    options |= base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES;
  if (pretty_print_)
    options |= base::JSONWriter::OPTIONS_PRETTY_PRINT;

  return base::JSONWriter::WriteWithOptions(root, options, json_string_);
}

JSONStringValueDeserializer::JSONStringValueDeserializer(
    const base::StringPiece& json_string,
    int options)
    : json_string_(json_string), options_(options) {}

JSONStringValueDeserializer::~JSONStringValueDeserializer() = default;

std::unique_ptr<Value> JSONStringValueDeserializer::Deserialize(
    int* error_code,
    std::string* error_str) {
  base::JSONReader::ValueWithError ret =
      base::JSONReader::ReadAndReturnValueWithError(json_string_, options_);
  if (ret.value)
    return base::Value::ToUniquePtrValue(std::move(*ret.value));

  if (error_code)
    *error_code = base::ValueDeserializer::kErrorCodeInvalidFormat;
  if (error_str)
    *error_str = std::move(ret.error_message);
  return nullptr;
}
