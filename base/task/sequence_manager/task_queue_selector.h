// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/pending_task.h"
#include "base/task/sequence_manager/task_queue_selector_logic.h"
#include "base/task/sequence_manager/work_queue_sets.h"

namespace base {
namespace sequence_manager {
namespace internal {

class AssociatedThreadId;

// TaskQueueSelector is used by the SchedulerHelper to enable prioritization
// of particular task queues.
class BASE_EXPORT TaskQueueSelector : public WorkQueueSets::Observer {
 public:
  explicit TaskQueueSelector(
      scoped_refptr<AssociatedThreadId> associated_thread);
  ~TaskQueueSelector() override;

  // Called to register a queue that can be selected. This function is called
  // on the main thread.
  void AddQueue(internal::TaskQueueImpl* queue);

  // The specified work will no longer be considered for selection. This
  // function is called on the main thread.
  void RemoveQueue(internal::TaskQueueImpl* queue);

  // Make |queue| eligible for selection. This function is called on the main
  // thread. Must only be called if |queue| is disabled.
  void EnableQueue(internal::TaskQueueImpl* queue);

  // Disable selection from |queue|. Must only be called if |queue| is enabled.
  void DisableQueue(internal::TaskQueueImpl* queue);

  // Called get or set the priority of |queue|.
  void SetQueuePriority(internal::TaskQueueImpl* queue,
                        TaskQueue::QueuePriority priority);

  // Called to choose the work queue from which the next task should be taken
  // and run. Return the queue to service if there is one or null otherwise.
  // This function is called on the main thread.
  WorkQueue* SelectWorkQueueToService();

  // Serialize the selector state for tracing.
  void AsValueInto(trace_event::TracedValue* state) const;

  class BASE_EXPORT Observer {
   public:
    virtual ~Observer() = default;

    // Called when |queue| transitions from disabled to enabled.
    virtual void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) = 0;
  };

  // Called once to set the Observer. This function is called
  // on the main thread. If |observer| is null, then no callbacks will occur.
  void SetTaskQueueSelectorObserver(Observer* observer);

  // Returns true if all the enabled work queues are empty. Returns false
  // otherwise.
  bool AllEnabledWorkQueuesAreEmpty() const;

  // WorkQueueSets::Observer implementation:
  void WorkQueueSetBecameEmpty(size_t set_index) override;
  void WorkQueueSetBecameNonEmpty(size_t set_index) override;

 protected:
  WorkQueue* ChooseOldestWithPriority(
      TaskQueue::QueuePriority priority,
      bool* out_chose_delayed_over_immediate) const;

  WorkQueueSets* delayed_work_queue_sets() { return &delayed_work_queue_sets_; }

  WorkQueueSets* immediate_work_queue_sets() {
    return &immediate_work_queue_sets_;
  }

  // Return true if |out_queue| contains the queue with the oldest pending task
  // from the set of queues of |priority|, or false if all queues of that
  // priority are empty. In addition |out_chose_delayed_over_immediate| is set
  // to true iff we chose a delayed work queue in favour of an immediate work
  // queue.  This method will force select an immediate task if those are being
  // starved by delayed tasks.
  void SetImmediateStarvationCountForTest(size_t immediate_starvation_count);

  // Maximum score to accumulate before high priority tasks are run even in
  // the presence of highest priority tasks.
  static const size_t kMaxHighPriorityStarvationScore = 3;

  // Maximum score to accumulate before normal priority tasks are run even in
  // the presence of higher priority tasks i.e. highest and high priority tasks.
  static const size_t kMaxNormalPriorityStarvationScore = 5;

  // Maximum score to accumulate before low priority tasks are run even in the
  // presence of highest, high, or normal priority tasks.
  static const size_t kMaxLowPriorityStarvationScore = 25;

  // Maximum number of delayed tasks tasks which can be run while there's a
  // waiting non-delayed task.
  static const size_t kMaxDelayedStarvationTasks = 3;

  // Because there are only a handful of priorities, we can get away with using
  // a very simple priority queue. This queue has a stable sorting order.
  // Note IDs must be in the range [0..TaskQueue::kQueuePriorityCount)
  class BASE_EXPORT SmallPriorityQueue {
   public:
    SmallPriorityQueue();

    bool empty() const { return size_ == 0; }

    int min_id() const { return index_to_id_[0]; };

    void insert(int64_t key, uint8_t id);

    void erase(uint8_t id);

    void ChangeMinKey(int64_t new_key);

    bool IsInQueue(uint8_t id) const {
      return id_to_index_[id] != kInvalidIndex;
    }

   private:
    static constexpr uint8_t kInvalidIndex = 255;

    size_t size_ = 0;

    // These are sorted in ascending order.
    int64_t keys_[TaskQueue::kQueuePriorityCount];
    uint8_t id_to_index_[TaskQueue::kQueuePriorityCount];
    uint8_t index_to_id_[TaskQueue::kQueuePriorityCount];
  };

 private:
  void ChangeSetIndex(internal::TaskQueueImpl* queue,
                      TaskQueue::QueuePriority priority);
  void AddQueueImpl(internal::TaskQueueImpl* queue,
                    TaskQueue::QueuePriority priority);
  void RemoveQueueImpl(internal::TaskQueueImpl* queue);

#if DCHECK_IS_ON() || !defined(NDEBUG)
  bool CheckContainsQueueForTest(const internal::TaskQueueImpl* queue) const;
#endif

  WorkQueue* ChooseOldestImmediateTaskWithPriority(
      TaskQueue::QueuePriority priority) const;

  WorkQueue* ChooseOldestDelayedTaskWithPriority(
      TaskQueue::QueuePriority priority) const;

  WorkQueue* ChooseOldestImmediateOrDelayedTaskWithPriority(
      TaskQueue::QueuePriority priority,
      bool* out_chose_delayed_over_immediate) const;

  // Returns the priority which is next after |priority|.
  static TaskQueue::QueuePriority NextPriority(
      TaskQueue::QueuePriority priority);

  // Returns true if there are pending tasks with priority |priority|.
  bool HasTasksWithPriority(TaskQueue::QueuePriority priority);

  scoped_refptr<AssociatedThreadId> associated_thread_;

  // Count of the number of sets (delayed or immediate) for each priority.
  // Should only contain 0, 1 or 2.
  std::array<int, TaskQueue::kQueuePriorityCount> non_empty_set_counts_ = {{0}};

  // The Priority sort key is adjusted based on these values. The idea being the
  // larger the adjustment, the more the queue can be starved before being
  // selected. The kControlPriority queues should run immediately so it always
  // has the lowest possible value. Conversely kBestEffortPriority queues should
  // only run if there's nothing else to do so they always have the highest
  // possible value.
  static constexpr const int64_t
      per_priority_starvation_tolerance_[TaskQueue::kQueuePriorityCount] = {
          // kControlPriority (unused)
          std::numeric_limits<int64_t>::min(),

          // kHighestPriority
          0,

          // kHighPriority
          TaskQueueSelector::per_priority_starvation_tolerance_[1] +
              kMaxHighPriorityStarvationScore,

          // kNormalPriority
          TaskQueueSelector::per_priority_starvation_tolerance_[2] +
              kMaxNormalPriorityStarvationScore,

          // kLowPriority
          TaskQueueSelector::per_priority_starvation_tolerance_[3] +
              kMaxLowPriorityStarvationScore,

          // kBestEffortPriority (unused)
          std::numeric_limits<int64_t>::max()};

  int64_t GetSortKeyForPriorty(size_t set_index) const;

  // Min priority queue of priorities, which is used to work out which priority
  // to run next.
  SmallPriorityQueue active_priorities_;

  // Each time we select a queue this is incremented. This forms the basis of
  // the |active_priorities_| sort key. I.e. when a priority becomes selectable
  // it's inserted into |active_priorities_| with a sort key of
  // |selection_count_| plus an adjustment from
  // |per_priority_starvation_tolerance_|. In theory this could wrap around and
  // start misbehaving but in typical usage that would take a great many years.
  int64_t selection_count_ = 0;

  WorkQueueSets delayed_work_queue_sets_;
  WorkQueueSets immediate_work_queue_sets_;
  size_t immediate_starvation_count_ = 0;

  Observer* task_queue_selector_observer_ = nullptr;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(TaskQueueSelector);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_SELECTOR_H_
