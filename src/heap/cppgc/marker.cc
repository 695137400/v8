// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marker.h"

#include <memory>

#include "include/cppgc/internal/process-heap.h"
#include "include/cppgc/platform.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/stats-collector.h"

#if defined(CPPGC_CAGED_HEAP)
#include "include/cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {
namespace internal {

namespace {

bool EnterIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                     HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    ProcessHeap::EnterIncrementalOrConcurrentMarking();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_marking_in_progress = true;
#endif
    return true;
  }
  return false;
}

bool ExitIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                    HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    ProcessHeap::ExitIncrementalOrConcurrentMarking();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_marking_in_progress = false;
#endif
    return true;
  }
  return false;
}

// Visit remembered set that was recorded in the generational barrier.
void VisitRememberedSlots(HeapBase& heap, MarkingState& marking_state) {
#if defined(CPPGC_YOUNG_GENERATION)
  for (void* slot : heap.remembered_slots()) {
    auto& slot_header = BasePage::FromInnerAddress(&heap, slot)
                            ->ObjectHeaderFromInnerAddress(slot);
    if (slot_header.IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(
        !header.IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>());

    void* value = *reinterpret_cast<void**>(slot);
    marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
  }
#endif
}

// Assumes that all spaces have their LABs reset.
void ResetRememberedSet(HeapBase& heap) {
#if defined(CPPGC_YOUNG_GENERATION)
  auto& local_data = heap.caged_heap().local_data();
  local_data.age_table.Reset(&heap.caged_heap().allocator());
  heap.remembered_slots().clear();
#endif
}

template <typename Worklist, typename Callback>
bool DrainWorklistWithDeadline(v8::base::TimeTicks deadline, Worklist* worklist,
                               Callback callback, int task_id) {
  const size_t kDeadlineCheckInterval = 1250;

  size_t processed_callback_count = 0;
  typename Worklist::View view(worklist, task_id);
  typename Worklist::EntryType item;
  while (view.Pop(&item)) {
    callback(item);
    if (++processed_callback_count == kDeadlineCheckInterval) {
      if (deadline <= v8::base::TimeTicks::Now()) {
        return false;
      }
      processed_callback_count = 0;
    }
  }
  return true;
}

}  // namespace

MarkerBase::IncrementalMarkingTask::IncrementalMarkingTask(MarkerBase* marker)
    : marker_(marker), handle_(Handle::NonEmptyTag{}) {}

// static
MarkerBase::IncrementalMarkingTask::Handle
MarkerBase::IncrementalMarkingTask::Post(MarkerBase* marker,
                                         v8::TaskRunner* runner) {
  auto task = std::make_unique<IncrementalMarkingTask>(marker);
  auto handle = task->handle_;
  runner->PostIdleTask(std::move(task));
  return handle;
}

void MarkerBase::IncrementalMarkingTask::Run(double deadline_in_seconds) {
  if (handle_.IsCanceled()) return;

  // Idle tasks are guaranteed to have no heap pointers on stack.
  const bool mark_complete = marker_->IncrementalMarkingStep(
      MarkingConfig::StackState::kNoHeapPointers,
      v8::base::TimeDelta::FromSecondsD(
          deadline_in_seconds -
          marker_->platform_->MonotonicallyIncreasingTime()));

  if (mark_complete) {
    marker_->FinalizeIncrementalMarking();
  } else {
    marker_->ScheduleIncrementalMarking();
  }
}

MarkerBase::MarkerBase(HeapBase& heap, cppgc::Platform* platform)
    : heap_(heap),
      platform_(platform),
      foreground_task_runner_(platform_->GetForegroundTaskRunner()),
      mutator_marking_state_(
          heap, marking_worklists_.marking_worklist(),
          marking_worklists_.not_fully_constructed_worklist(),
          marking_worklists_.weak_callback_worklist(),
          MarkingWorklists::kMutatorThreadId) {}

MarkerBase::~MarkerBase() {
  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  if (!marking_worklists_.not_fully_constructed_worklist()->IsEmpty()) {
#if DEBUG
    DCHECK_NE(MarkingConfig::StackState::kNoHeapPointers, config_.stack_state);
    MarkingWorklists::NotFullyConstructedItem item;
    MarkingWorklists::NotFullyConstructedWorklist::View view(
        marking_worklists_.not_fully_constructed_worklist(),
        MarkingWorklists::kMutatorThreadId);
    while (view.Pop(&item)) {
      const HeapObjectHeader& header =
          BasePage::FromPayload(item)->ObjectHeaderFromInnerAddress(
              static_cast<ConstAddress>(item));
      DCHECK(header.IsMarked());
    }
#else
    marking_worklists_.not_fully_constructed_worklist()->Clear();
#endif
  }
}

void MarkerBase::StartMarking(MarkingConfig config) {
  heap().stats_collector()->NotifyMarkingStarted();

  config_ = config;
  if (EnterIncrementalMarkingIfNeeded(config, heap())) {
    // Performing incremental or concurrent marking.
    // Scanning the stack is expensive so we only do it at the atomic pause.
    VisitRoots(MarkingConfig::StackState::kNoHeapPointers);
    ScheduleIncrementalMarking();
  }
}

void MarkerBase::EnterAtomicPause(MarkingConfig config) {
  if (ExitIncrementalMarkingIfNeeded(config_, heap())) {
    if (incremental_marking_handle_) incremental_marking_handle_.Cancel();
  }
  config_ = config;

  // VisitRoots also resets the LABs.
  VisitRoots(config_.stack_state);
  if (config_.stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    marking_worklists_.FlushNotFullyConstructedObjects();
  } else {
    MarkNotFullyConstructedObjects();
  }
}

void MarkerBase::LeaveAtomicPause() {
  ResetRememberedSet(heap());
  heap().stats_collector()->NotifyMarkingCompleted(
      mutator_marking_state_.marked_bytes());
}

void MarkerBase::FinishMarking(MarkingConfig config) {
  EnterAtomicPause(config);
  AdvanceMarkingWithDeadline(v8::base::TimeDelta::Max());
  LeaveAtomicPause();
}

void MarkerBase::ProcessWeakness() {
  heap().GetWeakPersistentRegion().Trace(&visitor());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  MarkingWorklists::WeakCallbackItem item;
  LivenessBroker broker = LivenessBrokerFactory::Create();
  MarkingWorklists::WeakCallbackWorklist::View view(
      marking_worklists_.weak_callback_worklist(),
      MarkingWorklists::kMutatorThreadId);
  while (view.Pop(&item)) {
    item.callback(broker, item.parameter);
  }
  // Weak callbacks should not add any new objects for marking.
  DCHECK(marking_worklists_.marking_worklist()->IsEmpty());
}

void MarkerBase::VisitRoots(MarkingConfig::StackState stack_state) {
  // Reset LABs before scanning roots. LABs are cleared to allow
  // ObjectStartBitmap handling without considering LABs.
  heap().object_allocator().ResetLinearAllocationBuffers();

  heap().GetStrongPersistentRegion().Trace(&visitor());
  if (stack_state != MarkingConfig::StackState::kNoHeapPointers) {
    heap().stack()->IteratePointers(&stack_visitor());
  }
  if (config_.collection_type == MarkingConfig::CollectionType::kMinor) {
    VisitRememberedSlots(heap(), mutator_marking_state_);
  }
}

void MarkerBase::ScheduleIncrementalMarking() {
  if (!platform_ || !foreground_task_runner_) return;
  DCHECK(!incremental_marking_handle_);
  incremental_marking_handle_ =
      IncrementalMarkingTask::Post(this, foreground_task_runner_.get());
}

void MarkerBase::FinalizeIncrementalMarking() {
  DCHECK(config_.marking_type != MarkingConfig::MarkingType::kAtomic);
  FinishMarking(config_);
}

bool MarkerBase::IncrementalMarkingStepForTesting(
    MarkingConfig::StackState stack_state, v8::base::TimeDelta deadline) {
  return IncrementalMarkingStep(stack_state, deadline);
}

bool MarkerBase::IncrementalMarkingStep(MarkingConfig::StackState stack_state,
                                        v8::base::TimeDelta deadline) {
  if (stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    marking_worklists_.FlushNotFullyConstructedObjects();
  }
  config_.stack_state = stack_state;

  return AdvanceMarkingWithDeadline(deadline);
}

bool MarkerBase::AdvanceMarkingWithDeadline(v8::base::TimeDelta duration) {
  v8::base::TimeTicks deadline = v8::base::TimeTicks::Now() + duration;

  do {
    // Convert |previously_not_fully_constructed_worklist_| to
    // |marking_worklist_|. This merely re-adds items with the proper
    // callbacks.
    if (!DrainWorklistWithDeadline(
            deadline,
            marking_worklists_.previously_not_fully_constructed_worklist(),
            [this](MarkingWorklists::NotFullyConstructedItem& item) {
              mutator_marking_state_.DynamicallyMarkAddress(
                  reinterpret_cast<ConstAddress>(item));
            },
            MarkingWorklists::kMutatorThreadId))
      return false;

    if (!DrainWorklistWithDeadline(
            deadline, marking_worklists_.marking_worklist(),
            [this](const MarkingWorklists::MarkingItem& item) {
              const HeapObjectHeader& header =
                  HeapObjectHeader::FromPayload(item.base_object_payload);
              DCHECK(!header.IsInConstruction<
                      HeapObjectHeader::AccessMode::kNonAtomic>());
              item.callback(&visitor(), item.base_object_payload);
              mutator_marking_state_.AccountMarkedBytes(header);
            },
            MarkingWorklists::kMutatorThreadId))
      return false;

    if (!DrainWorklistWithDeadline(
            deadline, marking_worklists_.write_barrier_worklist(),
            [this](HeapObjectHeader* header) {
              DCHECK(header);
              DCHECK(!header->IsInConstruction<
                      HeapObjectHeader::AccessMode::kNonAtomic>());
              const GCInfo& gcinfo =
                  GlobalGCInfoTable::GCInfoFromIndex(header->GetGCInfoIndex());
              gcinfo.trace(&visitor(), header->Payload());
              mutator_marking_state_.AccountMarkedBytes(*header);
            },
            MarkingWorklists::kMutatorThreadId))
      return false;
  } while (!marking_worklists_.marking_worklist()->IsLocalViewEmpty(
      MarkingWorklists::kMutatorThreadId));

  return true;
}

void MarkerBase::MarkNotFullyConstructedObjects() {
  MarkingWorklists::NotFullyConstructedItem item;
  MarkingWorklists::NotFullyConstructedWorklist::View view(
      marking_worklists_.not_fully_constructed_worklist(),
      MarkingWorklists::kMutatorThreadId);
  while (view.Pop(&item)) {
    conservative_visitor().TraceConservativelyIfNeeded(item);
  }
}

void MarkerBase::ClearAllWorklistsForTesting() {
  marking_worklists_.ClearForTesting();
}

Marker::Marker(HeapBase& heap, cppgc::Platform* platform)
    : MarkerBase(heap, platform),
      marking_visitor_(heap, mutator_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {}

}  // namespace internal
}  // namespace cppgc
