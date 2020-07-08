// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-heap.h"

#include <cstddef>
#include <cstring>

#include "include/v8.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/mutex.h"
#include "src/common/ptr-compr-inl.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/third-party/heap-api.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/snapshot/read-only-deserializer.h"

namespace v8 {
namespace internal {

namespace {
// Mutex used to ensure that ReadOnlyArtifacts creation is only done once.
base::LazyMutex read_only_heap_creation_mutex_ = LAZY_MUTEX_INITIALIZER;

// Weak pointer holding ReadOnlyArtifacts. ReadOnlyHeap::SetUp creates a
// std::shared_ptr from this when it attempts to reuse it. Since all Isolates
// hold a std::shared_ptr to this, the object is destroyed when no Isolates
// remain.
base::LazyInstance<std::weak_ptr<ReadOnlyArtifacts>>::type
    read_only_artifacts_ = LAZY_INSTANCE_INITIALIZER;

std::shared_ptr<ReadOnlyArtifacts> InitializeSharedReadOnlyArtifacts() {
  auto artifacts = std::make_shared<ReadOnlyArtifacts>();
  *read_only_artifacts_.Pointer() = artifacts;
  return artifacts;
}
}  // namespace

bool ReadOnlyHeap::IsSharedMemoryAvailable() {
  static bool shared_memory_allocation_supported =
      V8::GetCurrentPlatform()->GetPageAllocator()->CanAllocateSharedPages();
  return shared_memory_allocation_supported;
}

#ifdef V8_SHARED_RO_HEAP
#ifndef V8_COMPRESS_POINTERS
// This ReadOnlyHeap instance will only be accessed by Isolates that are already
// set up. As such it doesn't need to be guarded by a mutex or shared_ptrs,
// since an already set up Isolate will hold a shared_ptr to
// read_only_artifacts_.
ReadOnlyHeap* ReadOnlyHeap::shared_ro_heap_ = nullptr;
#endif

namespace {
void CopyAndRemapRoots(Address* src, Address* dst, Address new_base) {
#ifdef V8_TARGET_ARCH_64_BIT
  Address src_base = GetIsolateRoot(src[0]);
  for (size_t i = 0; i < ReadOnlyHeap::kEntriesCount; ++i) {
    dst[i] = src[i] - src_base + new_base;
  }
#else
  UNREACHABLE();
#endif
}
}  // namespace
#endif  // V8_SHARED_RO_HEAP

void ReadOnlyHeap::CopyRootsToIsolateRoots(Isolate* isolate) {
#ifdef V8_SHARED_RO_HEAP
#ifdef V8_COMPRESS_POINTERS
  auto isolate_ro_roots =
      isolate->roots_table().read_only_roots_begin().location();
  CopyAndRemapReadOnlyRoots(isolate_ro_roots,
                            GetIsolateRoot(GetIsolateRoot(isolate)));
#else
  void* const isolate_ro_roots =
      isolate->roots_table().read_only_roots_begin().location();
  std::memcpy(isolate_ro_roots, read_only_roots_,
              kEntriesCount * sizeof(Address));
#endif  // V8_COMPRESS_POINTERS
#else
  UNREACHABLE();
#endif  // V8_SHARED_RO_HEAP
}

void ReadOnlyHeap::CopyAndRemapReadOnlyRoots(Address* dst, Address new_base) {
#ifdef V8_SHARED_RO_HEAP
  CopyAndRemapRoots(read_only_roots_, dst, new_base);
#else
  UNREACHABLE();
#endif  // V8_SHARED_RO_HEAP
}

void ReadOnlyHeap::InitializeReadOnlyRoots(
    std::shared_ptr<ReadOnlyArtifacts> artifacts, Address new_base) {
#ifdef V8_SHARED_RO_HEAP
  CopyAndRemapRoots(artifacts->read_only_heap()->read_only_roots_,
                    read_only_roots_, new_base);
#else
  UNREACHABLE();
#endif  // V8_SHARED_RO_HEAP
}

// static
void ReadOnlyHeap::SetUp(Isolate* isolate, ReadOnlyDeserializer* des) {
  DCHECK_NOT_NULL(isolate);

  if (IsReadOnlySpaceShared()) {
    ReadOnlyHeap* ro_heap;
    if (des != nullptr) {
      bool read_only_heap_created = false;
      base::MutexGuard guard(read_only_heap_creation_mutex_.Pointer());
      std::shared_ptr<ReadOnlyArtifacts> artifacts =
          read_only_artifacts_.Get().lock();
      if (!artifacts) {
        artifacts = InitializeSharedReadOnlyArtifacts();
        ro_heap = CreateAndAttachToIsolate(isolate, artifacts);
        artifacts->InitializeChecksum(des);
        ro_heap->DeseralizeIntoIsolate(isolate, des);
        read_only_heap_created = true;
      } else {
        // With pointer compression, there is one ReadOnlyHeap per Isolate.
        ro_heap =
            ReadOnlyArtifacts::GetReadOnlyHeapForIsolate(artifacts, isolate);
        isolate->SetUpFromReadOnlyArtifacts(artifacts, ro_heap);
      }
      artifacts->VerifyChecksum(des, read_only_heap_created);
      ro_heap->CopyRootsToIsolateRoots(isolate);
    } else {
      // This path should only be taken in mksnapshot, should only be run once
      // before tearing down the Isolate that holds this ReadOnlyArtifacts and
      // is not thread-safe.
      std::shared_ptr<ReadOnlyArtifacts> artifacts =
          read_only_artifacts_.Get().lock();
      CHECK(!artifacts);
      artifacts = InitializeSharedReadOnlyArtifacts();

      ro_heap = CreateAndAttachToIsolate(isolate, artifacts);
      artifacts->VerifyChecksum(des, true);
    }
  } else {
    auto* ro_heap = new ReadOnlyHeap(new ReadOnlySpace(isolate->heap()));
    isolate->SetUpFromReadOnlyArtifacts(nullptr, ro_heap);
    if (des != nullptr) {
      ro_heap->DeseralizeIntoIsolate(isolate, des);
    }
  }
}

void ReadOnlyHeap::DeseralizeIntoIsolate(Isolate* isolate,
                                         ReadOnlyDeserializer* des) {
  DCHECK_NOT_NULL(des);
  des->DeserializeInto(isolate);
  InitFromIsolate(isolate);
}

void ReadOnlyHeap::OnCreateHeapObjectsComplete(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  InitFromIsolate(isolate);
}

ReadOnlyHeap::ReadOnlyHeap(ReadOnlyHeap* ro_heap, ReadOnlySpace* ro_space)
    : read_only_space_(ro_space),
      read_only_object_cache_(ro_heap->read_only_object_cache_) {
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());
#if defined(V8_SHARED_RO_HEAP) && defined(V8_COMPRESS_POINTERS)
  ro_heap->CopyAndRemapReadOnlyRoots(
      read_only_roots_, GetIsolateRoot(ro_space->FirstPageAddress()));
#endif  // V8_SHARED_RO_HEAP && V8_COMPRESS_POINTERS
}

// static
ReadOnlyHeap* ReadOnlyHeap::CreateAndAttachToIsolate(
    Isolate* isolate, std::shared_ptr<ReadOnlyArtifacts> artifacts) {
  DCHECK(IsReadOnlySpaceShared());
  std::unique_ptr<ReadOnlyHeap> ro_heap(
      new ReadOnlyHeap(new ReadOnlySpace(isolate->heap())));
#if defined(V8_SHARED_RO_HEAP) && !defined(V8_COMPRESS_POINTERS)
  // The global shared ReadOnlyHeap is only used without pointer compression.
  shared_ro_heap_ = ro_heap.get();
#endif  // V8_SHARED_RO_HEAP && !V8_COMPRESS_POINTERS
  artifacts->set_read_only_heap(std::move(ro_heap));
  isolate->SetUpFromReadOnlyArtifacts(artifacts, artifacts->read_only_heap());
  return artifacts->read_only_heap();
}

void ReadOnlyHeap::InitFromIsolate(Isolate* isolate) {
  DCHECK(!init_complete_);
  read_only_space_->ShrinkPages();
#ifdef V8_SHARED_RO_HEAP
  if (IsReadOnlySpaceShared()) {
    void* const isolate_ro_roots = reinterpret_cast<void*>(
        isolate->roots_table().read_only_roots_begin().address());
    std::memcpy(read_only_roots_, isolate_ro_roots,
                kEntriesCount * sizeof(Address));
    std::shared_ptr<ReadOnlyArtifacts> artifacts(
        *read_only_artifacts_.Pointer());

    read_only_space()->DetachPagesAndAddToArtifacts(artifacts);
    read_only_space_ = artifacts->shared_read_only_space();

#ifdef V8_COMPRESS_POINTERS
    auto* read_only_heap =
        new ReadOnlyHeap(this, isolate->heap()->read_only_space());
    isolate->set_read_only_heap(read_only_heap);

    DCHECK_EQ(*isolate->roots_table().read_only_roots_begin().location(),
              isolate->read_only_heap()->read_only_roots_[0]);

    // Confirm the canonical versions of the ReadOnlySpace/ReadOnlyHeap from the
    // ReadOnlyArtifacts are not accidentally present in a real Isolate (which
    // might destroy them) and the ReadOnlyHeaps and Spaces are correctly
    // associated with each other.
    DCHECK_NE(artifacts->shared_read_only_space(),
              isolate->heap()->read_only_space());
    DCHECK_NE(artifacts->read_only_heap(), isolate->read_only_heap());
    DCHECK_EQ(artifacts->read_only_heap()->read_only_space(),
              artifacts->shared_read_only_space());
    DCHECK_EQ(isolate->read_only_heap()->read_only_space(),
              isolate->heap()->read_only_space());
#endif  // V8_COMPRESS_POINTERS
  } else {
    read_only_space_->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);
  }
#else
  read_only_space_->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);
#endif  // V8_SHARED_RO_HEAP
  init_complete_ = true;
}

void ReadOnlyHeap::OnHeapTearDown(Heap* heap) {
  // When the ReadOnlyHeap and ReadOnlySpace are shared between Isolates, then
  // there's no need to tear them down as they have already been detached from
  // the originating Heap.
#if !defined(V8_SHARED_RO_HEAP) || defined(V8_COMPRESS_POINTERS)
  read_only_space_->TearDown(heap->memory_allocator());
  delete read_only_space_;
#endif
}

// static
void ReadOnlyHeap::PopulateReadOnlySpaceStatistics(
    SharedMemoryStatistics* statistics) {
  statistics->read_only_space_size_ = 0;
  statistics->read_only_space_used_size_ = 0;
  statistics->read_only_space_physical_size_ = 0;
  if (IsReadOnlySpaceShared()) {
    std::shared_ptr<ReadOnlyArtifacts> artifacts =
        read_only_artifacts_.Get().lock();
    if (artifacts) {
      auto ro_space = artifacts->shared_read_only_space();
      statistics->read_only_space_size_ = ro_space->CommittedMemory();
      statistics->read_only_space_used_size_ = ro_space->Size();
      statistics->read_only_space_physical_size_ =
          ro_space->CommittedPhysicalMemory();
    }
  }
}

// static
bool ReadOnlyHeap::Contains(Address address) {
  return BasicMemoryChunk::FromAddress(address)->InReadOnlySpace();
}

// static
bool ReadOnlyHeap::Contains(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return third_party_heap::Heap::InReadOnlySpace(object.address());
  } else {
    return BasicMemoryChunk::FromHeapObject(object)->InReadOnlySpace();
  }
}

Object* ReadOnlyHeap::ExtendReadOnlyObjectCache() {
  read_only_object_cache_.push_back(Smi::zero());
  return &read_only_object_cache_.back();
}

Object ReadOnlyHeap::cached_read_only_object(size_t i) const {
  DCHECK_LE(i, read_only_object_cache_.size());
  return read_only_object_cache_[i];
}

bool ReadOnlyHeap::read_only_object_cache_is_initialized() const {
  return read_only_object_cache_.size() > 0;
}

ReadOnlyHeapObjectIterator::ReadOnlyHeapObjectIterator(ReadOnlyHeap* ro_heap)
    : ReadOnlyHeapObjectIterator(ro_heap->read_only_space()) {}

ReadOnlyHeapObjectIterator::ReadOnlyHeapObjectIterator(ReadOnlySpace* ro_space)
    : ro_space_(ro_space),
      current_page_(V8_ENABLE_THIRD_PARTY_HEAP_BOOL
                        ? std::vector<ReadOnlyPage*>::iterator()
                        : ro_space->pages().begin()),
      current_addr_(V8_ENABLE_THIRD_PARTY_HEAP_BOOL
                        ? Address()
                        : (*current_page_)->address() +
                              MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(
                                  RO_SPACE)) {}

HeapObject ReadOnlyHeapObjectIterator::Next() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return HeapObject();  // Unsupported
  }

  if (current_page_ == ro_space_->pages().end()) {
    return HeapObject();
  }

  BasicMemoryChunk* current_page = *current_page_;
  for (;;) {
    Address end = current_page->address() + current_page->area_size() +
                  MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(RO_SPACE);
    DCHECK_LE(current_addr_, end);
    if (current_addr_ == end) {
      // Progress to the next page.
      ++current_page_;
      if (current_page_ == ro_space_->pages().end()) {
        return HeapObject();
      }
      current_page = *current_page_;
      current_addr_ =
          current_page->address() +
          MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(RO_SPACE);
    }

    if (current_addr_ == ro_space_->top() &&
        current_addr_ != ro_space_->limit()) {
      current_addr_ = ro_space_->limit();
      continue;
    }
    HeapObject object = HeapObject::FromAddress(current_addr_);
    const int object_size = object.Size();
    current_addr_ += object_size;

    if (object.IsFreeSpaceOrFiller()) {
      continue;
    }

    DCHECK_OBJECT_SIZE(object_size);
    return object;
  }
}

}  // namespace internal
}  // namespace v8
