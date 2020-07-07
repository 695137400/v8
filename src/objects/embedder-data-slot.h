// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_H_

#include <utility>

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/objects/slots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class EmbedderDataArray;
class JSObject;
class Object;

// An EmbedderDataSlot instance describes a kEmbedderDataSlotSize field ("slot")
// holding an embedder data which may contain raw aligned pointer or a tagged
// pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using respective load_XX() and
// store_XX() methods.
// Storing heap object through this slot may require triggering write barriers
// so this operation must be done via static store_tagged() methods.
class EmbedderDataSlot
    : public SlotBase<EmbedderDataSlot, Address, kTaggedSize> {
 public:
  EmbedderDataSlot() : SlotBase(kNullAddress) {}
  V8_INLINE EmbedderDataSlot(EmbedderDataArray array, int entry_index);
  V8_INLINE EmbedderDataSlot(JSObject object, int embedder_field_index);

#if defined(V8_TARGET_BIG_ENDIAN) && defined(V8_COMPRESS_POINTERS)
  static constexpr int kTaggedPayloadOffset = kTaggedSize;
#else
  static constexpr int kTaggedPayloadOffset = 0;
#endif

#ifdef V8_COMPRESS_POINTERS
  // The other part of the full pointer contains the index into the external
  // pointer table, encoded as SMI.
  static constexpr int kRawPayloadOffset = kTaggedSize - kTaggedPayloadOffset;
#endif
  static constexpr int kRequiredPtrAlignment = kSmiTagSize;

  // Opaque type used for storing raw embedder data.
  using RawData = Address;

  V8_INLINE void AllocateExternalPointerEntry(Isolate* isolate);

  V8_INLINE Object load_tagged() const;
  V8_INLINE void store_smi(Smi value);

  // Setting an arbitrary tagged value requires triggering a write barrier
  // which requires separate object and offset values, therefore these static
  // functions also has the target object parameter.
  static V8_INLINE void store_tagged(EmbedderDataArray array, int entry_index,
                                     Object value);
  static V8_INLINE void store_tagged(JSObject object, int embedder_field_index,
                                     Object value);

  // Tries reinterpret the value as an aligned pointer and sets *out_result to
  // the pointer-like value. Note, that some Smis could still look like an
  // aligned pointers.
  // Returns true on success.
  // Calling this method when the slot does not contain an aligned pointer is
  // undefined behaviour and may result in crashes.
  V8_INLINE bool ToAlignedPointer(const Isolate* isolate,
                                  void** out_result) const;

  // Call this function if you are not sure whether the slot contains an
  // external pointer or not. In case the slot contains an aligned pointer, this
  // method is guaranteed to suceed. In case it stores an internal value, this
  // call may still succeed and return an arbitrary value, however, it is
  // guaranteed not to crash.
  V8_INLINE bool ToAlignedPointerSafe(const Isolate* isolate,
                                      void** out_result) const;

  // Returns true if the pointer was successfully stored or false it the pointer
  // was improperly aligned.
  V8_INLINE V8_WARN_UNUSED_RESULT bool store_aligned_pointer(Isolate* isolate,
                                                             void* ptr);

  V8_INLINE RawData load_raw(Isolate* isolate,
                             const DisallowHeapAllocation& no_gc) const;
  V8_INLINE void store_raw(Isolate* isolate, RawData data,
                           const DisallowHeapAllocation& no_gc);

 private:
  // Stores given value to the embedder data slot in a concurrent-marker
  // friendly manner (tagged part of the slot is written atomically).
  V8_INLINE void gc_safe_store(Isolate* isolate, Address value);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
