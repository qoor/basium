// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/post_task_and_reply_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace base {
namespace internal {

namespace {

class PostTaskAndReplyTaskRunner : public internal::PostTaskAndReplyImpl {
 public:
  explicit PostTaskAndReplyTaskRunner(TaskRunner* destination)
      : destination_(destination) {}

 private:
  bool PostTask(const Location& from_here, OnceClosure task) override {
    return destination_->PostTask(from_here, std::move(task));
  }

  // Non-owning.
  TaskRunner* const destination_;
};

class ObjectToDelete : public RefCounted<ObjectToDelete> {
 public:
  // |delete_flag| is set to true when this object is deleted
  ObjectToDelete(bool* delete_flag) : delete_flag_(delete_flag) {
    EXPECT_FALSE(*delete_flag_);
  }

 private:
  friend class RefCounted<ObjectToDelete>;
  ~ObjectToDelete() { *delete_flag_ = true; }

  bool* const delete_flag_;

  DISALLOW_COPY_AND_ASSIGN(ObjectToDelete);
};

class MockObject {
 public:
  MockObject() = default;

  MOCK_METHOD1(Task, void(scoped_refptr<ObjectToDelete>));
  MOCK_METHOD1(Reply, void(scoped_refptr<ObjectToDelete>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObject);
};

class PostTaskAndReplyImplTest : public testing::Test {
 protected:
  PostTaskAndReplyImplTest() = default;

  void PostTaskAndReplyToMockObject() {
    // Expect the post to succeed.
    EXPECT_TRUE(
        PostTaskAndReplyTaskRunner(post_runner_.get())
            .PostTaskAndReply(
                FROM_HERE,
                BindOnce(&MockObject::Task, Unretained(&mock_object_),
                         MakeRefCounted<ObjectToDelete>(&delete_task_flag_)),
                BindOnce(&MockObject::Reply, Unretained(&mock_object_),
                         MakeRefCounted<ObjectToDelete>(&delete_reply_flag_))));

    // Expect the first task to be posted to |post_runner_|.
    EXPECT_TRUE(post_runner_->HasPendingTask());
    EXPECT_FALSE(reply_runner_->HasPendingTask());
    EXPECT_FALSE(delete_task_flag_);
    EXPECT_FALSE(delete_reply_flag_);
  }

  scoped_refptr<TestMockTimeTaskRunner> post_runner_ =
      MakeRefCounted<TestMockTimeTaskRunner>();
  scoped_refptr<TestMockTimeTaskRunner> reply_runner_ =
      MakeRefCounted<TestMockTimeTaskRunner>(
          TestMockTimeTaskRunner::Type::kBoundToThread);
  testing::StrictMock<MockObject> mock_object_;
  bool delete_task_flag_ = false;
  bool delete_reply_flag_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(PostTaskAndReplyImplTest);
};

}  // namespace

TEST_F(PostTaskAndReplyImplTest, PostTaskAndReply) {
  PostTaskAndReplyToMockObject();

  EXPECT_CALL(mock_object_, Task(_));
  post_runner_->RunUntilIdle();
  testing::Mock::VerifyAndClear(&mock_object_);
  // The task should have been deleted right after being run.
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_FALSE(delete_reply_flag_);

  // Expect the reply to be posted to |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_TRUE(reply_runner_->HasPendingTask());

  EXPECT_CALL(mock_object_, Reply(_));
  reply_runner_->RunUntilIdle();
  testing::Mock::VerifyAndClear(&mock_object_);
  EXPECT_TRUE(delete_task_flag_);
  // The reply should have been deleted right after being run.
  EXPECT_TRUE(delete_reply_flag_);

  // Expect no pending task in |post_runner_| and |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_FALSE(reply_runner_->HasPendingTask());
}

TEST_F(PostTaskAndReplyImplTest, PostTaskAndReplyDoesNotRun) {
  PostTaskAndReplyToMockObject();

  EXPECT_CALL(mock_object_, Task(_));
  post_runner_->RunUntilIdle();
  testing::Mock::VerifyAndClear(&mock_object_);
  // The task should have been deleted right after being run.
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_FALSE(delete_reply_flag_);

  // Expect the reply to be posted to |reply_runner_|.
  EXPECT_FALSE(post_runner_->HasPendingTask());
  EXPECT_TRUE(reply_runner_->HasPendingTask());

  // Clear the |reply_runner_| queue without running tasks. The reply callback
  // should be deleted.
  reply_runner_->ClearPendingTasks();
  EXPECT_TRUE(delete_task_flag_);
  EXPECT_TRUE(delete_reply_flag_);
}

}  // namespace internal
}  // namespace base
