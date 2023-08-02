// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/conservative-stack-visitor.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/marking-inl.h"
#include "src/objects/visitors.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/common/ptr-compr-inl.h"
#endif  // V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

ConservativeStackVisitor::ConservativeStackVisitor(Isolate* isolate,
                                                   RootVisitor* delegate)
    : cage_base_(isolate),
      delegate_(delegate),
      allocator_(isolate->heap()->memory_allocator()),
      collector_(delegate->collector()),
      stats_(isolate->heap()->css_stats()) {}

ConservativeStackVisitor::ConservativeStackVisitor(Isolate* isolate,
                                                   GarbageCollector collector)
    : cage_base_(isolate),
      delegate_(nullptr),
      allocator_(isolate->heap()->memory_allocator()),
      collector_(collector),
      stats_(isolate->heap()->css_stats()) {}

Address ConservativeStackVisitor::FindBasePtrForMarking(
    Address maybe_inner_ptr) const {
  // Check if the pointer is contained by a normal or large page owned by this
  // heap. Bail out if it is not.
  const BasicMemoryChunk* chunk =
      allocator_->LookupChunkContainingAddress(maybe_inner_ptr);
  if (chunk == nullptr) {
    stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::PAGE_NOT_FOUND);
    return kNullAddress;
  }
  DCHECK(chunk->Contains(maybe_inner_ptr));
  // If it is contained in a large page, we want to mark the only object on it.
  if (chunk->IsLargePage()) {
    stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::LARGE_PAGE);
    // This could be simplified if we could guarantee that there are no free
    // space or filler objects in large pages. A few cctests violate this now.
    HeapObject obj(static_cast<const LargePage*>(chunk)->GetObject());
    PtrComprCageBase cage_base{chunk->heap()->isolate()};
    if (IsFreeSpaceOrFiller(obj, cage_base)) {
      stats_->AddPointer(obj.address(), measure_css::Stats::FREE_SPACE);
      return kNullAddress;
    } else {
      return obj.address();
    }
  }
  // Otherwise, we have a pointer inside a normal page.
  stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::NORMAL_PAGE);
  const Page* page = static_cast<const Page*>(chunk);
  // If it is not in the young generation and we're only interested in young
  // generation pointers, we must ignore it.
  if (Heap::IsYoungGenerationCollector(collector_) &&
      !page->InYoungGeneration()) {
    stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::NOT_IN_YOUNG);
    return kNullAddress;
  }
  // If it is in the young generation "from" semispace, it is not used and we
  // must ignore it, as its markbits may not be clean.
  if (page->IsFromPage()) {
    stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::YOUNG_FROM);
    return kNullAddress;
  }
  // Try to find the address of a previous valid object on this page.
  Address base_ptr = MarkingBitmap::FindPreviousObjectForConservativeMarking(
      page, maybe_inner_ptr, stats_);
  // If the markbit is set, then we have an object that does not need to be
  // marked.
  if (base_ptr == kNullAddress) {
    stats_->AddPointer(maybe_inner_ptr, measure_css::Stats::ALREADY_MARKED);
    return kNullAddress;
  }
  // Iterate through the objects in the page forwards, until we find the object
  // containing maybe_inner_ptr.
  DCHECK_LE(base_ptr, maybe_inner_ptr);
  PtrComprCageBase cage_base{page->heap()->isolate()};
  for (int iterations = 0; true; ++iterations) {
    HeapObject obj(HeapObject::FromAddress(base_ptr));
    const int size = obj.Size(cage_base);
    DCHECK_LT(0, size);
    if (maybe_inner_ptr < base_ptr + size) {
      stats_->AddValue(maybe_inner_ptr, measure_css::Stats::ITER_FORWARD,
                       iterations);
      if (IsFreeSpaceOrFiller(obj, cage_base)) {
        stats_->AddPointer(obj.address(), measure_css::Stats::FREE_SPACE);
        return kNullAddress;
      } else {
        return base_ptr;
      }
    }
    base_ptr += size;
    DCHECK_LT(base_ptr, page->area_end());
  }
}

void ConservativeStackVisitor::VisitPointer(const void* pointer) {
  auto address = reinterpret_cast<Address>(const_cast<void*>(pointer));
  stats_->AddPointer(address, measure_css::Stats::PRIMARY);
  VisitConservativelyIfPointer<false>(address);
#ifdef V8_COMPRESS_POINTERS
  V8HeapCompressionScheme::ProcessIntermediatePointers(
      cage_base_, address,
      [this](Address ptr) { VisitConservativelyIfPointer<true>(ptr); });
#endif  // V8_COMPRESS_POINTERS
}

template <bool is_known_to_be_in_cage>
void ConservativeStackVisitor::VisitConservativelyIfPointer(Address address) {
  stats_->AddPointer(address, measure_css::Stats::SECONDARY);
#ifdef V8_COMPRESS_POINTERS
  if constexpr (!is_known_to_be_in_cage) {
    // Bail out immediately if the pointer is not in the cage.
    if (V8HeapCompressionScheme::GetPtrComprCageBaseAddress(address) !=
        cage_base_.address()) {
      stats_->AddPointer(address, measure_css::Stats::OUT_OF_CAGE);
      return;
    }
  }
  DCHECK_EQ(V8HeapCompressionScheme::GetPtrComprCageBaseAddress(address),
            cage_base_.address());
#endif  // V8_COMPRESS_POINTERS
  // Bail out immediately if the pointer is not in the space managed by the
  // allocator.
  if (allocator_->IsOutsideAllocatedSpace(address)) {
    DCHECK_EQ(nullptr, allocator_->LookupChunkContainingAddress(address));
    stats_->AddPointer(address, measure_css::Stats::OUT_OF_ALLOCATED_SPACE);
    return;
  }
  // Proceed with inner-pointer resolution.
  Address base_ptr = FindBasePtrForMarking(address);
  if (base_ptr == kNullAddress) return;
  HeapObject obj = HeapObject::FromAddress(base_ptr);
  Object root = obj;
  DCHECK_NOT_NULL(delegate_);
  delegate_->VisitRootPointer(Root::kConservativeStackRoots, nullptr,
                              FullObjectSlot(&root));
  // Check that the delegate visitor did not modify the root slot.
  DCHECK_EQ(root, obj);
}

void measure_css::ObjectStats::Clear() {
  base::MutexGuard lock(&mutex_);
  objects_.clear();
}

bool measure_css::ObjectStats::IsClear() const {
  base::MutexGuard lock(&mutex_);
  return objects_.empty();
}

void measure_css::ObjectStats::AddObject(Address p) {
  base::MutexGuard lock(&mutex_);
  auto it = objects_.find(p);
  CHECK_EQ(objects_.end(), it);
  objects_.insert(p);
}

bool measure_css::ObjectStats::LookupObject(Address p) const {
  base::MutexGuard lock(&mutex_);
  auto it = objects_.find(p);
  return it != objects_.end();
}

void measure_css::Stats::PointerStats::PrintNVPOn(std::ostream& out) const {
  out << "{\"total\": " << count_ << ",\"unique\": " << unique_;
  if (id_ >= measure_css::Stats::LARGE_PAGE) {
    std::unordered_set<Address> young_pages, old_pages;
    for (auto [pointer, multiplicity] : histogram_) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(pointer);
      Address page = reinterpret_cast<Address>(chunk);
      if (chunk->InNewSpace()) {
        young_pages.insert(page);
      } else {
        old_pages.insert(page);
      }
    }
    out << ",\"young pages\": " << young_pages.size()
        << ",\"old pages\": " << old_pages.size();
  }
  if (id_ >= measure_css::Stats::FALSE_POSITIVE) {
    out << ",\"size\": " << size_;
  }
  if (v8_flags.trace_css_histograms > 0) {
    const int number_of_buckets = v8_flags.trace_css_histograms;
    std::vector<int> h(number_of_buckets);
    for (auto [pointer, multiplicity] : histogram_) {
      // Assume pointer compression; this works regardless but is probably
      // bogus.
      int bucket = static_cast<int64_t>(static_cast<uint32_t>(pointer)) *
                   number_of_buckets / 0x100000000;
      h[bucket] += multiplicity;
    }
    bool first = true;
    out << ",\"histogram\": [";
    for (int c : h) {
      out << (first ? "" : ",") << c;
      first = false;
    }
    out << "]";
  }
  out << "}";
}

template <typename T>
void measure_css::Stats::ValueStats<T>::PrintNVPOn(std::ostream& out) const {
  out << "{\"count\": " << count_;
  if (count_ > 0) {
    out << ",\"sum\": " << sum_ << ",\"min\": " << min_ << ",\"max\": " << max_
        << ",\"avg\": " << double(sum_) / count_;
  }
  out << "}";
}

void measure_css::ObjectStats::PrintNVPOn(std::ostream& out) const {
  out << "{\"total\": " << objects_.size() << "}";
}

namespace {
bool IsBlackAllocated(Address ptr, int size) {
  Page* p = Page::FromAddress(ptr);
  MarkingBitmap::MarkBitIndex start = MarkingBitmap::AddressToIndex(ptr);
  MarkingBitmap::MarkBitIndex end =
      MarkingBitmap::LimitAddressToIndex(ptr + size);
  return p->marking_bitmap()->AllBitsSetInRange(start, end);
}
}  // namespace

void measure_css::Stats::AddPointer(Address p, CounterId id) {
  pointers_[id].AddSample(p);
  if (id != ALREADY_MARKED && id != FULL_ALREADY_MARKED &&
      id != FULL_NOT_ALREADY_MARKED && id != YOUNG_ALREADY_MARKED &&
      id != YOUNG_NOT_ALREADY_MARKED)
    return;
  auto [base_ptr, size] = FindObject(p);
  if (base_ptr == kNullAddress) return;  // Free space or filler.
  bool in_marked = marked_objects_.LookupObject(base_ptr);
  if (id == FULL_NOT_ALREADY_MARKED || id == YOUNG_NOT_ALREADY_MARKED) {
    // The object should definitely be marked now.
    CHECK(in_marked);
    pointers_[FALSE_POSITIVE].AddSample(base_ptr, size);
  } else if (id == ALREADY_MARKED && !in_marked) {
    CHECK(IsBlackAllocated(base_ptr, size));
    pointers_[BLACK_ALLOCATED].AddSample(base_ptr, size);
    pointers_[WOULD_BE_PINNED].AddSample(base_ptr, size);
  } else {
    CHECK(in_marked);
    pointers_[WOULD_BE_PINNED].AddSample(base_ptr, size);
  }
}

void measure_css::Stats::AddValue(Address p, ValueId id, int64_t value) {
  value_[id].AddSample(value);
}

std::pair<Address, int> measure_css::Stats::FindObject(
    Address maybe_inner_ptr) const {
  const BasicMemoryChunk* chunk =
      heap_->memory_allocator()->LookupChunkContainingAddress(maybe_inner_ptr);
  CHECK_NOT_NULL(chunk);
  CHECK(chunk->Contains(maybe_inner_ptr));
  if (chunk->IsLargePage()) {
    HeapObject obj(static_cast<const LargePage*>(chunk)->GetObject());
    PtrComprCageBase cage_base{chunk->heap()->isolate()};
    int size = obj.Size(cage_base);
    if (IsFreeSpaceOrFiller(obj, cage_base)) {
      return std::make_pair(kNullAddress, size);
    } else {
      return std::make_pair(obj.address(), size);
    }
  }
  const Page* page = static_cast<const Page*>(chunk);
  Address base_ptr = page->area_start();
  DCHECK_LE(base_ptr, maybe_inner_ptr);
  PtrComprCageBase cage_base{page->heap()->isolate()};
  while (true) {
    HeapObject obj(HeapObject::FromAddress(base_ptr));
    const int size = obj.Size(cage_base);
    DCHECK_LT(0, size);
    if (maybe_inner_ptr < base_ptr + size) {
      if (IsFreeSpaceOrFiller(obj, cage_base)) {
        return std::make_pair(kNullAddress, size);
      } else {
        return std::make_pair(base_ptr, size);
      }
    }
    base_ptr += size;
    DCHECK_LT(base_ptr, page->area_end());
  }
}

void measure_css::Stats::PrintNVPOn(std::ostream& out) const {
  out << "{\"primary\": " << pointers_[PRIMARY]
      << ",\"secondary\": " << pointers_[SECONDARY]
      << ",\"out of cage\": " << pointers_[OUT_OF_CAGE]
      << ",\"out of allocated space\": " << pointers_[OUT_OF_ALLOCATED_SPACE]
      << ",\"page not found\": " << pointers_[PAGE_NOT_FOUND]
      << ",\"large page\": " << pointers_[LARGE_PAGE]
      << ",\"normal page\": " << pointers_[NORMAL_PAGE]
      << ",\"free space\": " << pointers_[FREE_SPACE]
      << ",\"not in young\": " << pointers_[NOT_IN_YOUNG]
      << ",\"young from\": " << pointers_[YOUNG_FROM]
      << ",\"already marked\": " << pointers_[ALREADY_MARKED]
      << ",\"full, should not mark\": " << pointers_[FULL_SHOULD_NOT_MARK]
      << ",\"full, not already marked\": " << pointers_[FULL_NOT_ALREADY_MARKED]
      << ",\"full, already marked\": " << pointers_[FULL_ALREADY_MARKED]
      << ",\"young, should not mark\": " << pointers_[YOUNG_SHOULD_NOT_MARK]
      << ",\"young, not already marked\": "
      << pointers_[YOUNG_NOT_ALREADY_MARKED]
      << ",\"young, already marked\": " << pointers_[YOUNG_ALREADY_MARKED]
      << ",\"iter backward 1\": " << value_[ITER_BACKWARD_1]
      << ",\"iter backward 2\": " << value_[ITER_BACKWARD_2]
      << ",\"iter forward\": " << value_[ITER_FORWARD]
      << ",\"marked objects\": " << marked_objects_
      << ",\"false positive\": " << pointers_[FALSE_POSITIVE]
      << ",\"would be pinned\": " << pointers_[WOULD_BE_PINNED]
      << ",\"black allocated\": " << pointers_[BLACK_ALLOCATED] << "}";
}

}  // namespace internal
}  // namespace v8
