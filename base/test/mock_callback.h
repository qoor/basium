// This file was GENERATED by command:
//     pump.py mock_callback.h.pump
// DO NOT EDIT BY HAND!!!

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Analogous to GMock's built-in MockFunction, but for base::Callback instead of
// std::function. It takes the full callback type as a parameter, so that it can
// support both OnceCallback and RepeatingCallback. Furthermore, this file
// defines convenience typedefs in the form of MockOnceCallback<Signature>,
// MockRepeatingCallback<Signature>, MockOnceClosure and MockRepeatingClosure.
//
// Use:
//   using FooCallback = base::RepeatingCallback<int(std::string)>;
//
//   TEST(FooTest, RunsCallbackWithBarArgument) {
//     base::MockCallback<FooCallback> callback;
//     EXPECT_CALL(callback, Run("bar")).WillOnce(Return(1));
//     Foo(callback.Get());
//   }
//
// Or equivalently:
//
//   TEST(FooTest, RunsCallbackWithBarArgument) {
//     base::MockRepeatingCallback<int(std::string)> callback;
//     EXPECT_CALL(callback, Run("bar")).WillOnce(Return(1));
//     Foo(callback.Get());
//   }
//
//
// Can be used with StrictMock and NiceMock. Caller must ensure that it outlives
// any base::Callback obtained from it.

#ifndef BASE_TEST_MOCK_CALLBACK_H_
#define BASE_TEST_MOCK_CALLBACK_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

// clang-format off

template <typename F>
class MockCallback;

template <typename Signature>
using MockOnceCallback = MockCallback<OnceCallback<Signature>>;
template <typename Signature>
using MockRepeatingCallback = MockCallback<RepeatingCallback<Signature>>;

using MockOnceClosure = MockCallback<OnceClosure>;
using MockRepeatingClosure = MockCallback<RepeatingClosure>;

template <typename R>
class MockCallback<RepeatingCallback<R()>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD0_T(Run, R());

  RepeatingCallback<R()> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R>
class MockCallback<OnceCallback<R()>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD0_T(Run, R());

  OnceCallback<R()> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1>
class MockCallback<RepeatingCallback<R(A1)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD1_T(Run, R(A1));

  RepeatingCallback<R(A1)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1>
class MockCallback<OnceCallback<R(A1)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD1_T(Run, R(A1));

  OnceCallback<R(A1)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2>
class MockCallback<RepeatingCallback<R(A1, A2)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD2_T(Run, R(A1, A2));

  RepeatingCallback<R(A1, A2)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2>
class MockCallback<OnceCallback<R(A1, A2)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD2_T(Run, R(A1, A2));

  OnceCallback<R(A1, A2)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3>
class MockCallback<RepeatingCallback<R(A1, A2, A3)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD3_T(Run, R(A1, A2, A3));

  RepeatingCallback<R(A1, A2, A3)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3>
class MockCallback<OnceCallback<R(A1, A2, A3)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD3_T(Run, R(A1, A2, A3));

  OnceCallback<R(A1, A2, A3)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD4_T(Run, R(A1, A2, A3, A4));

  RepeatingCallback<R(A1, A2, A3, A4)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4>
class MockCallback<OnceCallback<R(A1, A2, A3, A4)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD4_T(Run, R(A1, A2, A3, A4));

  OnceCallback<R(A1, A2, A3, A4)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD5_T(Run, R(A1, A2, A3, A4, A5));

  RepeatingCallback<R(A1, A2, A3, A4, A5)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD5_T(Run, R(A1, A2, A3, A4, A5));

  OnceCallback<R(A1, A2, A3, A4, A5)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5, A6)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD6_T(Run, R(A1, A2, A3, A4, A5, A6));

  RepeatingCallback<R(A1, A2, A3, A4, A5, A6)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5, A6)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD6_T(Run, R(A1, A2, A3, A4, A5, A6));

  OnceCallback<R(A1, A2, A3, A4, A5, A6)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD7_T(Run, R(A1, A2, A3, A4, A5, A6, A7));

  RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5, A6, A7)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD7_T(Run, R(A1, A2, A3, A4, A5, A6, A7));

  OnceCallback<R(A1, A2, A3, A4, A5, A6, A7)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD8_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8));

  RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD8_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8));

  OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD9_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8, A9));

  RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD9_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8, A9));

  OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9,
    typename A10>
class MockCallback<RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9,
    A10)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD10_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10));

  RepeatingCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)> Get() {
    return BindRepeating(&MockCallback::Run, Unretained(this));
  }
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9,
    typename A10>
class MockCallback<OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>> {
 public:
  MockCallback() = default;

MockCallback(const MockCallback&) = delete;
MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD10_T(Run, R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10));

  OnceCallback<R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)> Get() {
    return BindOnce(&MockCallback::Run, Unretained(this));
  }
};

// clang-format on

}  // namespace base

#endif  // BASE_TEST_MOCK_CALLBACK_H_
