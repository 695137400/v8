// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/etw-jit.h"

#include "src/diagnostics/etw-metadata.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
#endif

#ifndef V8_ETW_GUID
#define V8_ETW_GUID \
  0x57277741, 0x3638, 0x4A4B, 0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C
#endif

namespace v8 {
namespace internal {
namespace ETWJITInterface {

TRACELOGGING_DECLARE_PROVIDER(g_v8Provider);

TRACELOGGING_DEFINE_PROVIDER(g_v8Provider, "V8.js", (V8_ETW_GUID));

ScriptMapType* isolate_script_map = new ScriptMapType();

void EventHandler(const JitCodeEvent* event) {
  if (event->code_type != v8::JitCodeEvent::CodeType::JIT_CODE) return;
  if (event->type != v8::JitCodeEvent::EventType::CODE_ADDED) return;

  int name_len = static_cast<int>(event->name.len);
  // Note: event->name.str is not null terminated.
  std::wstring method_name(name_len + 1, '\0');
  MultiByteToWideChar(
      CP_UTF8, 0, event->name.str, name_len,
      // Const cast needed as building with C++14 (not const in >= C++17)
      const_cast<LPWSTR>(method_name.data()),
      static_cast<int>(method_name.size()));

  void* script_context = static_cast<void*>(event->isolate);
  auto script = event->script;
  int script_id = 0;
  if (!script.IsEmpty()) {
    // if the first time seeing this source file, log the SourceLoad event
    script_id = script->GetId();
    auto& script_map = (*isolate_script_map)[script_context];
    if (script_map.find(script_id) == script_map.end()) {
      script_map.insert(script_id);

      auto script_name = script->GetScriptName();
      if (script_name->IsString()) {
        auto v8str_name = script_name.As<v8::String>();
        std::wstring wstr_name(v8str_name->Length(), L'\0');
        // On Windows wchar_t == uint16_t. const_Cast needed for C++14.
        uint16_t* wstr_data = const_cast<uint16_t*>(
            reinterpret_cast<const uint16_t*>(wstr_name.data()));
        v8str_name->Write(event->isolate, wstr_data);
      }

      constexpr static auto source_load_event_meta =
          EventMetadata(kSourceLoadEventID, kJScriptRuntimeKeyword);
      constexpr static auto source_load_event_fields = EventFields(
          "SourceLoad", Field("SourceID", TlgInUINT64),
          Field("ScriptContextID", TlgInPOINTER),
          Field("SourceFlags", TlgInUINT32), Field("Url", TlgInUNICODESTRING));
      LogEventData(g_v8Provider, &source_load_event_meta,
                   &source_load_event_fields,
                   (uint64_t)script_id,  // ScriptID
                   script_context,
                   (uint32_t)0,  // SourceFlags
                   script_name);
    }
  }

  constexpr static auto method_load_event_meta =
      EventMetadata(kMethodLoadEventID, kJScriptRuntimeKeyword);
  constexpr static auto method_load_event_fields = EventFields(
      "MethodLoad", Field("ScriptContextID", TlgInPOINTER),
      Field("MethodStartAddress", TlgInPOINTER),
      Field("MethodSize", TlgInUINT64), Field("MethodID", TlgInUINT32),
      Field("MethodFlags", TlgInUINT16),
      Field("MethodAddressRangeID", TlgInUINT16),
      Field("SourceID", TlgInUINT64), Field("Line", TlgInUINT32),
      Field("Column", TlgInUINT32), Field("MethodName", TlgInUNICODESTRING));

  LogEventData(g_v8Provider, &method_load_event_meta, &method_load_event_fields,
               script_context, event->code_start, (uint64_t)event->code_len,
               (uint32_t)0,               // MethodId
               (uint16_t)0,               // MethodFlags
               (uint16_t)0,               // MethodAddressRangeId
               (uint64_t)script_id,       // ScriptID
               (uint32_t)0, (uint32_t)0,  // Line & Column
               method_name);
}

}  // namespace ETWJITInterface
}  // namespace internal
}  // namespace v8
