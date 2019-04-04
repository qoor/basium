// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLER_IMPL_H_
#define BASE_PROFILER_STACK_SAMPLER_IMPL_H_

#include <memory>

#include "base/base_export.h"
#include "base/profiler/frame.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_sampler.h"

namespace base {

class ThreadDelegate;
class Unwinder;

// Cross-platform stack sampler implementation. Delegates to ThreadDelegate for
// platform-specific implementation.
class BASE_EXPORT StackSamplerImpl : public StackSampler {
 public:
  StackSamplerImpl(std::unique_ptr<ThreadDelegate> delegate,
                   std::unique_ptr<Unwinder> native_unwinder,
                   ModuleCache* module_cache,
                   StackSamplerTestDelegate* test_delegate = nullptr);
  ~StackSamplerImpl() override;

  StackSamplerImpl(const StackSamplerImpl&) = delete;
  StackSamplerImpl& operator=(const StackSamplerImpl&) = delete;

  // Records a set of frames and returns them.
  void RecordStackFrames(StackBuffer* stack_buffer,
                         ProfileBuilder* profile_builder) override;

 private:
  bool CopyStack(StackBuffer* stack_buffer,
                 uintptr_t* stack_top,
                 ProfileBuilder* profile_builder,
                 RegisterContext* thread_context);

  std::vector<Frame> WalkStack(RegisterContext* thread_context,
                               uintptr_t stack_top);

  const std::unique_ptr<ThreadDelegate> thread_delegate_;
  const std::unique_ptr<Unwinder> native_unwinder_;
  ModuleCache* const module_cache_;
  StackSamplerTestDelegate* const test_delegate_;
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLER_IMPL_H_
