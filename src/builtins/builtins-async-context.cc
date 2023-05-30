// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/codegen/compiler.h"
#include "src/logging/counters.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-async-context-inl.h"
#include "src/objects/ordered-hash-table.h"

namespace v8 {
namespace internal {

BUILTIN(AsyncLocalPrototypeRun) {
  HandleScope scope(isolate);

  Handle<Object> receiver = args.receiver();
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<Object> target = args.atOrUndefined(isolate, 2);

  Factory* factory = isolate->factory();

  // 1. Let asyncLocal be the this value.
  // 2. Perform ? RequireInternalSlot(asyncLocal, [[AsyncLocalName]]).
  if (!receiver->IsJSAsyncLocal()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver));
  }
  Handle<JSAsyncLocal> async_local = Handle<JSAsyncLocal>::cast(receiver);

  if (!target->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, target));
  }

  // 3. Let previousContextMapping be AsyncContextSnapshot().
  Handle<NativeContext> context = isolate->native_context();
  Handle<HeapObject> snapshot =
      Handle<HeapObject>(context->async_context_store(), isolate);
  // 4. Let asyncContextMapping be a new empty List.
  Handle<OrderedHashMap> async_context_store = factory->NewOrderedHashMap();
  // 5. For each Async Context Mapping Record p of previousContextMapping, do
  //   a. If SameValueZero(p.[[AsyncContextKey]], asyncLocal) is false, then
  //     i. Let q be the Async Context Mapping Record { [[AsyncContextKey]]:
  //     p.[[AsyncContextKey]], [[AsyncContextValue]]: p.[[AsyncContextValue]]
  //     }. ii. Append q to asyncContextMapping.
  if (!snapshot->IsUndefined()) {
    Handle<OrderedHashMap> snapshot_map =
        Handle<OrderedHashMap>::cast(snapshot);
    async_context_store->CopyElements(isolate, 0, *snapshot_map, 0,
                                      snapshot_map->length(),
                                      WriteBarrierMode::SKIP_WRITE_BARRIER);
  }
  // 6. Assert: asyncContextMapping does not contain an Async Context Mapping
  // Record whose [[AsyncContextKey]] is asyncLocal.
  // 7. Let p be the Async Context Mapping Record { [[AsyncContextKey]]:
  // asyncLocal, [[AsyncContextValue]]: value }.
  // 8. Append p to asyncContextMapping.
  OrderedHashMap::Add(isolate, async_context_store, async_local, value);
  // 9. AsyncContextSwap(asyncContextMapping).
  context->set_async_context_store(*async_context_store);

  base::ScopedVector<Handle<Object>> argv(std::max(0, args.length() - 3));
  for (int i = 3; i < args.length(); ++i) {
    argv[i - 3] = args.at(i);
  }

  Handle<Object> result;
  // 10. Let result be Completion(Call(func, undefined, args)).
  // TODO(abotella): Call the function with `args` (arguments 3+ passed to this
  // function).
  if (!Execution::Call(isolate, target,  // Handle<JSFunction>::cast(target),
                       isolate->factory()->undefined_value(), argv.length(),
                       argv.data())
           .ToHandle(&result)) {
    DCHECK((isolate)->has_pending_exception());
    // 11. AsyncContextSwap(previousContextMapping).
    context->set_async_context_store(*snapshot);
    // 12. Return result.
    return ReadOnlyRoots(isolate).exception();
  }

  // 11. AsyncContextSwap(previousContextMapping).
  context->set_async_context_store(*snapshot);
  // 12. Return result.
  return *result;
}

BUILTIN(AsyncLocalPrototypeGet) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();

  if (!receiver->IsJSAsyncLocal()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver));
  }
  Handle<JSAsyncLocal> async_local = Handle<JSAsyncLocal>::cast(receiver);
  Handle<NativeContext> context = isolate->native_context();

  Handle<Object> snapshot =
      Handle<Object>(context->async_context_store(), isolate);
  if (snapshot->IsUndefined()) {
    return async_local->defaultValue();
  }

  Handle<OrderedHashMap> async_context_store =
      Handle<OrderedHashMap>::cast(snapshot);
  InternalIndex found = async_context_store->FindEntry(isolate, *async_local);
  if (found.is_not_found()) {
    return async_local->defaultValue();
  }

  return async_context_store->ValueAt(found);
}

BUILTIN(AsyncSnapshotConstructor) {
  HandleScope scope(isolate);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->AsyncSnapshot_string()));
  }

  // 2. Let snapshotMapping be AsyncContextSnapshot();
  Handle<NativeContext> context = isolate->native_context();
  Handle<HeapObject> snapshot_mapping =
      Handle<HeapObject>(context->async_context_store(), isolate);

  // 3. Let asyncSnapshot be ? OrdinaryCreateFromConstructor(NewTarget,
  // "%AsyncSnapshot.prototype%", « [[AsyncSnapshotMapping]] »).
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSAsyncSnapshot> async_snapshot =
      Handle<JSAsyncSnapshot>::cast(result);

  // 4. Set asyncSnapshot.[[AsyncSnapshotMapping]] to snapshotMapping.
  async_snapshot->set_snapshot(*snapshot_mapping);

  // 5. Return asyncSnapshot.
  return *async_snapshot;
}

BUILTIN(AsyncSnapshotPrototypeRestore) {
  HandleScope scope(isolate);

  Handle<Object> receiver = args.receiver();
  Handle<Object> func = args.atOrUndefined(isolate, 1);

  // 1. Let asyncSnapshot be the this value.
  // 2. Perform ? RequireInternalSlot(asyncSnapshot, [[AsyncSnapshotMapping]]).
  if (!receiver->IsJSAsyncSnapshot()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver));
  }

  if (!func->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, func));
  }

  // 3. Let snapshotMapping be asyncSnapshot.[[AsyncSnapshotMapping]].
  HeapObject snapshot_mapping =
      Handle<JSAsyncSnapshot>::cast(receiver)->snapshot();

  // 4. Let previousContextMapping be AsyncContextSnapshot().
  Handle<NativeContext> context = isolate->native_context();
  Handle<HeapObject> previous_context_mapping =
      Handle<HeapObject>(context->async_context_store(), isolate);

  // 5. AsyncContextSwap(snapshotMapping).
  context->set_async_context_store(snapshot_mapping);

  base::ScopedVector<Handle<Object>> argv(std::max(0, args.length() - 2));
  for (int i = 2; i < args.length(); ++i) {
    argv[i - 2] = args.at(i);
  }

  Handle<Object> result;
  // 6. Let result be Completion(Call(func, undefined, args)).
  // TODO(abotella): Call the function with `args` (arguments 3+ passed to this
  // function).
  if (!Execution::Call(isolate, func, isolate->factory()->undefined_value(),
                       argv.length(), argv.data())
           .ToHandle(&result)) {
    DCHECK((isolate)->has_pending_exception());
    // 7. AsyncContextSwap(previousContextMapping).
    context->set_async_context_store(*previous_context_mapping);
    // 12. Return result.
    return ReadOnlyRoots(isolate).exception();
  }

  // 11. AsyncContextSwap(previousContextMapping).
  context->set_async_context_store(*previous_context_mapping);
  // 12. Return result.
  return *result;
}

}  // namespace internal
}  // namespace v8
