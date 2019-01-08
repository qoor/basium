// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_selector.h"

#include <utility>

#include "base/logging.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/traced_value.h"

namespace base {
namespace sequence_manager {
namespace internal {

constexpr const int64_t TaskQueueSelector::per_priority_starvation_tolerance_[];

TaskQueueSelector::TaskQueueSelector(
    scoped_refptr<AssociatedThreadId> associated_thread)
    : associated_thread_(std::move(associated_thread)),
      delayed_work_queue_sets_("delayed", this),
      immediate_work_queue_sets_("immediate", this) {}

TaskQueueSelector::~TaskQueueSelector() = default;

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, TaskQueue::kNormalPriority);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    RemoveQueueImpl(queue);
  }
}

void TaskQueueSelector::EnableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, queue->GetQueuePriority());
  if (task_queue_selector_observer_)
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
}

void TaskQueueSelector::DisableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(!queue->IsQueueEnabled());
  RemoveQueueImpl(queue);
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK_LT(priority, TaskQueue::kQueuePriorityCount);
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    ChangeSetIndex(queue, priority);
  } else {
    // Disabled queue is not in any set so we can't use ChangeSetIndex here
    // and have to assign priority for the queue itself.
    queue->delayed_work_queue()->AssignSetIndex(priority);
    queue->immediate_work_queue()->AssignSetIndex(priority);
  }
  DCHECK_EQ(priority, queue->GetQueuePriority());
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::kQueuePriorityCount);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

void TaskQueueSelector::AddQueueImpl(internal::TaskQueueImpl* queue,
                                     TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.AddQueue(queue->delayed_work_queue(), priority);
  immediate_work_queue_sets_.AddQueue(queue->immediate_work_queue(), priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::ChangeSetIndex(internal::TaskQueueImpl* queue,
                                       TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.ChangeSetIndex(queue->delayed_work_queue(),
                                          priority);
  immediate_work_queue_sets_.ChangeSetIndex(queue->immediate_work_queue(),
                                            priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::RemoveQueueImpl(internal::TaskQueueImpl* queue) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());

#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
}

int64_t TaskQueueSelector::GetSortKeyForPriorty(size_t set_index) const {
  switch (set_index) {
    case TaskQueue::kControlPriority:
      return std::numeric_limits<int64_t>::min();

    case TaskQueue::kBestEffortPriority:
      return std::numeric_limits<int64_t>::max();

    default:
      return selection_count_ + per_priority_starvation_tolerance_[set_index];
  }
}

void TaskQueueSelector::WorkQueueSetBecameEmpty(size_t set_index) {
  non_empty_set_counts_[set_index]--;
  DCHECK_GE(non_empty_set_counts_[set_index], 0);

  // There are no delayed or immediate tasks for |set_index| so remove from
  // |active_priorities_|.
  if (non_empty_set_counts_[set_index] == 0)
    active_priorities_.erase(set_index);
}

void TaskQueueSelector::WorkQueueSetBecameNonEmpty(size_t set_index) {
  non_empty_set_counts_[set_index]++;
  DCHECK_LE(non_empty_set_counts_[set_index], 2);

  // There is now a delayed or an immediate task for |set_index|, so add to
  // |active_priorities_|.
  if (non_empty_set_counts_[set_index] == 1)
    active_priorities_.insert(GetSortKeyForPriorty(set_index), set_index);
}

WorkQueue* TaskQueueSelector::ChooseOldestImmediateTaskWithPriority(
    TaskQueue::QueuePriority priority) const {
  return immediate_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority) const {
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestImmediateOrDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate) const {
  DCHECK_EQ(*out_chose_delayed_over_immediate, false);
  EnqueueOrder immediate_enqueue_order;
  WorkQueue* immediate_queue =
      immediate_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
          priority, &immediate_enqueue_order);
  if (immediate_queue) {
    EnqueueOrder delayed_enqueue_order;
    WorkQueue* delayed_queue =
        delayed_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
            priority, &delayed_enqueue_order);
    if (!delayed_queue)
      return immediate_queue;

    if (immediate_enqueue_order < delayed_enqueue_order) {
      return immediate_queue;
    } else {
      *out_chose_delayed_over_immediate = true;
      return delayed_queue;
    }
  }
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate) const {
  // Select an immediate work queue if we are starving immediate tasks.
  if (immediate_starvation_count_ >= kMaxDelayedStarvationTasks) {
    WorkQueue* queue = ChooseOldestImmediateTaskWithPriority(priority);
    if (queue)
      return queue;
    return ChooseOldestDelayedTaskWithPriority(priority);
  }
  return ChooseOldestImmediateOrDelayedTaskWithPriority(
      priority, out_chose_delayed_over_immediate);
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool TaskQueueSelector::CheckContainsQueueForTest(
    const internal::TaskQueueImpl* queue) const {
  bool contains_delayed_work_queue =
      delayed_work_queue_sets_.ContainsWorkQueueForTest(
          queue->delayed_work_queue());

  bool contains_immediate_work_queue =
      immediate_work_queue_sets_.ContainsWorkQueueForTest(
          queue->immediate_work_queue());

  DCHECK_EQ(contains_delayed_work_queue, contains_immediate_work_queue);
  return contains_delayed_work_queue;
}
#endif

WorkQueue* TaskQueueSelector::SelectWorkQueueToService() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);

  if (active_priorities_.empty())
    return nullptr;

  selection_count_++;

  // Select the priority from which we will select a task. Usually this is
  // the highest priority for which we have work, unless we are starving a lower
  // priority.
  size_t set_index = active_priorities_.min_id();
  bool chose_delayed_over_immediate = false;
  WorkQueue* queue =
      ChooseOldestWithPriority(static_cast<TaskQueue::QueuePriority>(set_index),
                               &chose_delayed_over_immediate);

  // If we still have any tasks remaining for |set_index| then adjust it's
  // sort key.
  if (active_priorities_.IsInQueue(set_index))
    active_priorities_.ChangeMinKey(GetSortKeyForPriorty(set_index));

  if (chose_delayed_over_immediate) {
    immediate_starvation_count_++;
  } else {
    immediate_starvation_count_ = 0;
  }
  return queue;
}

void TaskQueueSelector::AsValueInto(trace_event::TracedValue* state) const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  state->SetInteger("immediate_starvation_count", immediate_starvation_count_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::AllEnabledWorkQueuesAreEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  return active_priorities_.empty();
}

void TaskQueueSelector::SetImmediateStarvationCountForTest(
    size_t immediate_starvation_count) {
  immediate_starvation_count_ = immediate_starvation_count;
}

bool TaskQueueSelector::HasTasksWithPriority(
    TaskQueue::QueuePriority priority) {
  return !delayed_work_queue_sets_.IsSetEmpty(priority) ||
         !immediate_work_queue_sets_.IsSetEmpty(priority);
}

TaskQueueSelector::SmallPriorityQueue::SmallPriorityQueue() {
  for (size_t i = 0; i < TaskQueue::kQueuePriorityCount; i++) {
    id_to_index_[i] = kInvalidIndex;
  }
}

void TaskQueueSelector::SmallPriorityQueue::insert(int64_t key, uint8_t id) {
  DCHECK_LE(size_, TaskQueue::kQueuePriorityCount);
  DCHECK_LT(id, TaskQueue::kQueuePriorityCount);
  DCHECK(!IsInQueue(id));
  // Insert while keeping |keys_| sorted.
  size_t i = size_;
  while (i > 0 && key < keys_[i - 1]) {
    keys_[i] = keys_[i - 1];
    uint8_t moved_id = index_to_id_[i - 1];
    index_to_id_[i] = moved_id;
    id_to_index_[moved_id] = i;
    i--;
  }
  keys_[i] = key;
  index_to_id_[i] = id;
  id_to_index_[id] = i;
  size_++;
}

void TaskQueueSelector::SmallPriorityQueue::erase(uint8_t id) {
  DCHECK_NE(size_, 0u);
  DCHECK_LT(id, TaskQueue::kQueuePriorityCount);
  DCHECK(IsInQueue(id));
  // Erase while keeping |keys_| sorted.
  size_--;
  for (size_t i = id_to_index_[id]; i < size_; i++) {
    keys_[i] = keys_[i + 1];
    uint8_t moved_id = index_to_id_[i + 1];
    index_to_id_[i] = moved_id;
    id_to_index_[moved_id] = i;
  }
  id_to_index_[id] = kInvalidIndex;
}

void TaskQueueSelector::SmallPriorityQueue::ChangeMinKey(int64_t new_key) {
  DCHECK_NE(size_, 0u);
  uint8_t id = index_to_id_[0];
  size_t i = 0;
  while ((i + 1) < size_ && keys_[i + 1] < new_key) {
    keys_[i] = keys_[i + 1];
    uint8_t moved_id = index_to_id_[i + 1];
    index_to_id_[i] = moved_id;
    id_to_index_[moved_id] = i;
    i++;
  }
  keys_[i] = new_key;
  index_to_id_[i] = id;
  id_to_index_[id] = i;
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
