// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_STUB_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_STUB_H_

#include <stddef.h>

#include <string>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/values.h"

#define TRACE_STR_COPY(str) str
#define TRACE_ID_WITH_SCOPE(scope, ...) 0
#define TRACE_ID_GLOBAL(id) 0
#define TRACE_ID_LOCAL(id) 0

namespace trace_event_internal {

const unsigned long long kNoId = 0;

template <typename... Args>
void Ignore(Args&&... args) {}

struct IgnoredValue {
  template <typename... Args>
  IgnoredValue(Args&&... args) {}
};

}  // namespace trace_event_internal

#define INTERNAL_TRACE_IGNORE(...) \
  (false ? trace_event_internal::Ignore(__VA_ARGS__) : (void)0)

#define INTERNAL_TRACE_EVENT_ADD(...) INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(...) INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(...) INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_TASK_EXECUTION(...) INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_LOG_MESSAGE(...) INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(...) \
  INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(...) \
  INTERNAL_TRACE_IGNORE(__VA_ARGS__)
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMPS(...) \
  INTERNAL_TRACE_IGNORE(__VA_ARGS__)

#define TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION \
  trace_event_internal::IgnoredValue

#define TRACE_ID_MANGLE(val) (val)

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(cat) INTERNAL_TRACE_IGNORE(cat);
#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE() false

#define TRACE_EVENT_API_CURRENT_THREAD_ID 0

namespace base {
namespace trace_event {

class BASE_EXPORT ConvertableToTraceFormat {
 public:
  ConvertableToTraceFormat() = default;
  virtual ~ConvertableToTraceFormat();

  // Append the class info to the provided |out| string. The appended
  // data must be a valid JSON object. Strings must be properly quoted, and
  // escaped. There is no processing applied to the content after it is
  // appended.
  virtual void AppendAsTraceFormat(std::string* out) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConvertableToTraceFormat);
};

class BASE_EXPORT TracedValue : public ConvertableToTraceFormat {
 public:
  explicit TracedValue(size_t capacity = 0) {}

  void EndDictionary() {}
  void EndArray() {}

  void SetInteger(const char* name, int value) {}
  void SetDouble(const char* name, double value) {}
  void SetBoolean(const char* name, bool value) {}
  void SetString(const char* name, base::StringPiece value) {}
  void SetValue(const char* name, TracedValue* value) {}
  void BeginDictionary(const char* name) {}
  void BeginArray(const char* name) {}

  void SetIntegerWithCopiedName(base::StringPiece name, int value) {}
  void SetDoubleWithCopiedName(base::StringPiece name, double value) {}
  void SetBooleanWithCopiedName(base::StringPiece name, bool value) {}
  void SetStringWithCopiedName(base::StringPiece name,
                               base::StringPiece value) {}
  void SetValueWithCopiedName(base::StringPiece name, TracedValue* value) {}
  void BeginDictionaryWithCopiedName(base::StringPiece name) {}
  void BeginArrayWithCopiedName(base::StringPiece name) {}

  void AppendInteger(int) {}
  void AppendDouble(double) {}
  void AppendBoolean(bool) {}
  void AppendString(base::StringPiece) {}
  void BeginArray() {}
  void BeginDictionary() {}

  void AppendAsTraceFormat(std::string* out) const override;
};

class BASE_EXPORT TracedValueJSON : public TracedValue {
 public:
  explicit TracedValueJSON(size_t capacity = 0) : TracedValue(capacity) {}

  std::unique_ptr<base::Value> ToBaseValue() const { return nullptr; }
  std::string ToJSON() const { return ""; }
  std::string ToFormattedJSON() const { return ""; }
};

class BASE_EXPORT BlameContext {
 public:
  BlameContext(const char* category,
               const char* name,
               const char* type,
               const char* scope,
               int64_t id,
               const BlameContext* parent_context) {}

  void Initialize() {}
  void Enter() {}
  void Leave() {}
  void TakeSnapshot() {}

  const char* category() const { return nullptr; }
  const char* name() const { return nullptr; }
  const char* type() const { return nullptr; }
  const char* scope() const { return nullptr; }
  int64_t id() const { return 0; }
};

struct MemoryDumpArgs;
class ProcessMemoryDump;

class BASE_EXPORT MemoryDumpProvider {
 public:
  virtual ~MemoryDumpProvider();

  virtual bool OnMemoryDump(const MemoryDumpArgs& args,
                            ProcessMemoryDump* pmd) = 0;

 protected:
  MemoryDumpProvider() = default;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_STUB_H_
