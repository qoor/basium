// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
#define BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_

#include <type_traits>

#include "base/callback.h"
#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"

namespace base {
namespace internal {

// Exists to reduce template bloat.
class BASE_EXPORT ThenAndCatchExecutorCommon {
 public:
  ThenAndCatchExecutorCommon(internal::CallbackBase&& resolve_executor,
                             internal::CallbackBase&& reject_executor) noexcept
      : resolve_callback_(std::move(resolve_executor)),
        reject_callback_(std::move(reject_executor)) {
    DCHECK(!resolve_callback_.is_null() || !reject_callback_.is_null());
  }

  ~ThenAndCatchExecutorCommon() = default;

  // PromiseExecutor:
  bool IsCancelled() const;
  PromiseExecutor::PrerequisitePolicy GetPrerequisitePolicy() const;

  using ExecuteCallback = void (*)(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* callback);

  void Execute(AbstractPromise* promise,
               ExecuteCallback execute_then,
               ExecuteCallback execute_catch);

  // If |executor| is null then the value of |arg| is moved or copied into
  // |result| and true is returned. Otherwise false is returned.
  static bool ProcessNullCallback(const CallbackBase& executor,
                                  AbstractPromise* arg,
                                  AbstractPromise* result);

  CallbackBase resolve_callback_;
  CallbackBase reject_callback_;
};

// Tag signals no callback which is used to eliminate dead code.
struct NoCallback {};

template <typename ResolveOnceCallback,
          typename RejectOnceCallback,
          typename ArgResolve,
          typename ArgReject,
          typename ResolveStorage,
          typename RejectStorage>
class ThenAndCatchExecutor {
 public:
  using ResolveReturnT =
      typename CallbackTraits<ResolveOnceCallback>::ReturnType;
  using RejectReturnT = typename CallbackTraits<RejectOnceCallback>::ReturnType;
  using PrerequisiteCouldResolve =
      std::integral_constant<bool,
                             !std::is_same<ArgResolve, NoCallback>::value>;
  using PrerequisiteCouldReject =
      std::integral_constant<bool, !std::is_same<ArgReject, NoCallback>::value>;

  ThenAndCatchExecutor(CallbackBase&& resolve_callback,
                       CallbackBase&& reject_callback) noexcept
      : common_(std::move(resolve_callback), std::move(reject_callback)) {}

  bool IsCancelled() const { return common_.IsCancelled(); }

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PromiseExecutor::PrerequisitePolicy::kAll;

  using ExecuteCallback = ThenAndCatchExecutorCommon::ExecuteCallback;

  void Execute(AbstractPromise* promise) {
    return common_.Execute(promise, &ExecuteThen, &ExecuteCatch);
  }

#if DCHECK_IS_ON()
  PromiseExecutor::ArgumentPassingType ResolveArgumentPassingType() const {
    return common_.resolve_callback_.is_null()
               ? PromiseExecutor::ArgumentPassingType::kNoCallback
               : CallbackTraits<ResolveOnceCallback>::argument_passing_type;
  }

  PromiseExecutor::ArgumentPassingType RejectArgumentPassingType() const {
    return common_.reject_callback_.is_null()
               ? PromiseExecutor::ArgumentPassingType::kNoCallback
               : CallbackTraits<RejectOnceCallback>::argument_passing_type;
  }

  bool CanResolve() const {
    return (!common_.resolve_callback_.is_null() &&
            PromiseCallbackTraits<ResolveReturnT>::could_resolve) ||
           (!common_.reject_callback_.is_null() &&
            PromiseCallbackTraits<RejectReturnT>::could_resolve);
  }

  bool CanReject() const {
    return (!common_.resolve_callback_.is_null() &&
            PromiseCallbackTraits<ResolveReturnT>::could_reject) ||
           (!common_.reject_callback_.is_null() &&
            PromiseCallbackTraits<RejectReturnT>::could_reject);
  }
#endif

 private:
  static void ExecuteThen(AbstractPromise* prerequisite,
                          AbstractPromise* promise,
                          CallbackBase* resolve_callback) {
    ExecuteThenInternal(prerequisite, promise, resolve_callback,
                        PrerequisiteCouldResolve());
  }

  static void ExecuteCatch(AbstractPromise* prerequisite,
                           AbstractPromise* promise,
                           CallbackBase* reject_callback) {
    ExecuteCatchInternal(prerequisite, promise, reject_callback,
                         PrerequisiteCouldReject());
  }

  static void ExecuteThenInternal(AbstractPromise* prerequisite,
                                  AbstractPromise* promise,
                                  CallbackBase* resolve_callback,
                                  std::true_type can_resolve) {
    RunHelper<ResolveOnceCallback, Resolved<ArgResolve>, ResolveStorage,
              RejectStorage>::
        Run(std::move(*static_cast<ResolveOnceCallback*>(resolve_callback)),
            prerequisite, promise);
  }

  static void ExecuteThenInternal(AbstractPromise* prerequisite,
                                  AbstractPromise* promise,
                                  CallbackBase* resolve_callback,
                                  std::false_type can_resolve) {
    // |prerequisite| can't resolve so don't generate dead code.
  }

  static void ExecuteCatchInternal(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* reject_callback,
                                   std::true_type can_reject) {
    RunHelper<RejectOnceCallback, Rejected<ArgReject>, ResolveStorage,
              RejectStorage>::
        Run(std::move(*static_cast<RejectOnceCallback*>(reject_callback)),
            prerequisite, promise);
  }

  static void ExecuteCatchInternal(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* reject_callback,
                                   std::false_type can_reject) {
    // |prerequisite| can't reject so don't generate dead code.
  }

  ThenAndCatchExecutorCommon common_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
