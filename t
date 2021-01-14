
[38;5;4m[38;5;4msrc/compiler/access-info.cc[0m[0m
[38;5;4m───────────────────────────────────────────────────────────────────────────────────────────────────[0m
[38;5;4m─────────────────────────────────────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m MinimorphicLoadPropertyAccessInfo [38;5;149mAccessInfoFactory::ComputePropertyAccessInfo[38;5;231m([0m[0m [38;5;4m│[0m
[38;5;4m─────────────────────────────────────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m529[0m
[38;5;231m   DCHECK(feedback.handler()->IsSmi());[0m                                                             
[38;5;231m   [38;5;81mint[38;5;231m [38;5;208mhandler[38;5;231m [38;5;203m=[38;5;231m Smi::cast([38;5;203m*[38;5;231mfeedback.handler()).value();[0m                                            
[38;5;231m   [38;5;81mbool[38;5;231m is_inobject [38;5;203m=[38;5;231m LoadHandler::IsInobjectBits::decode(handler);[0m                                 
[48;5;52m   bool is_double = LoadHandler::IsDoubleBits::decode(handler);[0m[48;5;52m                                     [0m
[38;5;231m   [38;5;81mint[38;5;231m offset [38;5;203m=[38;5;231m LoadHandler::FieldIndexBits::decode(handler) [38;5;203m*[38;5;231m [38;5;141mkTaggedSize[38;5;231m;[0m                         
[48;5;52m   Representation field_rep =[0m[48;5;52m                                                                       [0m
[48;5;52m       is_double ? Representation::Double() : Representation::Tagged();[0m[48;5;52m                             [0m
[48;5;52m   Type field_type = is_double ? Type::Number() : Type::Any();[0m[48;5;52m                                      [0m
[48;5;22;38;5;231m   Type field_type [38;5;203m=[38;5;231m Type::NonInternal(); [38;5;242m// whats the diff between Any and NonInternal?[0m[48;5;22m            [0m
[48;5;22;38;5;231m   [38;5;81mbool[38;5;231m representation [38;5;203m=[38;5;231m LoadHandler::IsSmiBits::decode(handler);[0m[48;5;22m                                   [0m
[48;5;22;38;5;231m [0m[48;5;22m                                                                                                   [0m
[48;5;22;38;5;231m   [38;5;242m// TODO(gsathya): Can this be none?[0m[48;5;22m                                                              [0m
[48;5;22;38;5;231m   DCHECK([38;5;203m![38;5;231mrepresentation.IsNone());[0m[48;5;22m                                                                [0m
[48;5;22;38;5;231m [0m[48;5;22m                                                                                                   [0m
[48;5;22;38;5;231m   [38;5;203mif[38;5;231m (representation.IsSmi()) {[0m[48;5;22m                                                                    [0m
[48;5;22;38;5;231m     field_type [38;5;203m=[38;5;231m Type::SignedSmall();[0m[48;5;22m                                                              [0m
[48;5;22;38;5;231m   } [38;5;203melse[38;5;231m [38;5;203mif[38;5;231m (representation.IsDouble()) {[0m[48;5;22m                                                          [0m
[48;5;22;38;5;231m     field_type [38;5;203m=[38;5;231m type_cache_->kFloat64;[0m[48;5;22m                                                            [0m
[48;5;22;38;5;231m   }[0m[48;5;22m                                                                                                [0m
[48;5;22;38;5;231m [0m[48;5;22m                                                                                                   [0m
[38;5;231m   [38;5;203mreturn[38;5;231m MinimorphicLoadPropertyAccessInfo::DataField(offset, is_inobject,[0m                         
[48;5;52m                                                       field_rep, field_type);[0m[48;5;52m                      [0m
[48;5;22;38;5;231m                                                       representation, field_type);[0m[48;5;22m                 [0m
[38;5;231m }[0m                                                                                                  
[38;5;231m [0m                                                                                                   
[38;5;231m PropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo([0m                                   

[38;5;4m[38;5;4msrc/ic/handler-configuration-inl.h[0m[0m
[38;5;4m───────────────────────────────────────────────────────────────────────────────────────────────────[0m
[38;5;4m───────────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m [38;5;149mHandle[38;5;203m<[38;5;231mSmi[38;5;203m>[38;5;231m LoadHandler:[38;5;203m:[38;5;231mLoadSlow(Isolate[38;5;203m*[38;5;231m isolate) {[0m[0m [38;5;4m│[0m
[38;5;4m───────────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m50[0m
[38;5;231m   [38;5;203mreturn[38;5;231m handle(Smi:[38;5;203m:[38;5;231mFromInt(config), isolate);[0m                                                    
[38;5;231m }[0m                                                                                                  
[38;5;231m [0m                                                                                                   
[48;5;52m Handle<Smi> LoadHandler::LoadField(Isolate* isolate, FieldIndex field_index[48;5;124m) {[0m[48;5;52m                     [0m
[48;5;22;38;5;231m [38;5;149mHandle[38;5;203m<[38;5;231mSmi[38;5;203m>[38;5;231m LoadHandler:[38;5;203m:[38;5;231mLoadField(Isolate[38;5;203m*[38;5;231m isolate, FieldIndex field_index[48;5;28m,[0m[48;5;22m                       [0m
[48;5;22;38;5;231m                                    Representation representation) {[0m[48;5;22m                                [0m
[38;5;231m   [38;5;81mint[38;5;231m config [38;5;203m=[38;5;231m KindBits:[38;5;203m:[38;5;231mencode([38;5;141mkField[38;5;231m) [38;5;203m|[38;5;231m[0m                                                          
[38;5;231m                IsInobjectBits:[38;5;203m:[38;5;231mencode(field_index.is_inobject()) [38;5;203m|[38;5;231m[0m                                 
[38;5;231m                IsDoubleBits:[38;5;203m:[38;5;231mencode(field_index.is_double()) [38;5;203m|[38;5;231m[0m                                     
[48;5;52m                FieldIndexBits::encode(field_index.index())[48;5;124m;[0m[48;5;52m                                        [0m
[48;5;22;38;5;231m                FieldIndexBits:[38;5;203m:[38;5;231mencode(field_index.index())[48;5;28m [38;5;203m|[38;5;231m[0m[48;5;22m                                       [0m
[48;5;22;38;5;231m                IsSmiBits:[38;5;203m:[38;5;231mencode(representation.kind() [38;5;203m==[38;5;231m Representation:[38;5;203m:[38;5;141mkSmi[38;5;231m);[0m[48;5;22m                   [0m
[38;5;231m   [38;5;203mreturn[38;5;231m handle(Smi:[38;5;203m:[38;5;231mFromInt(config), isolate);[0m                                                    
[38;5;231m }[0m                                                                                                  
[38;5;231m [0m                                                                                                   

[38;5;4m[38;5;4msrc/ic/handler-configuration.h[0m[0m
[38;5;4m───────────────────────────────────────────────────────────────────────────────────────────────────[0m
[38;5;4m────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m class LoadHandler final [38;5;203m:[38;5;231m public DataHandler {[0m[0m [38;5;4m│[0m
[38;5;4m────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m80[0m
[38;5;231m   [38;5;242m// +1 here is to cover all possible JSObject header sizes.[0m                                       
[38;5;231m   using FieldIndexBits [38;5;203m=[38;5;231m[0m                                                                           
[38;5;231m       IsDoubleBits:[38;5;203m:[38;5;231mNext[38;5;203m<[38;5;81munsigned[38;5;231m, [38;5;141mkDescriptorIndexBitCount[38;5;231m [38;5;203m+[38;5;231m [38;5;141m1[38;5;203m>[38;5;231m;[0m                                  
[48;5;22;38;5;231m   using IsSmiBits [38;5;203m=[38;5;231m FieldIndexBits:[38;5;203m:[38;5;231mNext[38;5;203m<[38;5;81mbool[38;5;231m, [38;5;141m1[38;5;203m>[38;5;231m;[0m[48;5;22m                                                 [0m
[48;5;22;38;5;231m [38;5;242m//  using FieldTypeBits = RepresentationBits::Next<FieldType::Kind, 3>;[0m[48;5;22m                            [0m
[38;5;231m   [38;5;242m// Make sure we don't overflow the smi.[0m                                                          
[48;5;52m   STATIC_ASSERT([48;5;124mFieldIndexBits[48;5;52m::kLastUsedBit < kSmiValueSize);[0m[48;5;52m                                     [0m
[48;5;22;38;5;231m   STATIC_ASSERT([48;5;28mIsSmiBits[48;5;22m:[38;5;203m:[38;5;141mkLastUsedBit[38;5;231m [38;5;203m<[38;5;231m [38;5;141mkSmiValueSize[38;5;231m);[0m[48;5;22m                                          [0m
[38;5;231m [0m                                                                                                   
[38;5;231m   [38;5;242m//[0m                                                                                               
[38;5;231m   [38;5;242m// Encoding when KindBits contains kElement or kIndexedString.[0m                                   
[38;5;4m────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m class LoadHandler final [38;5;203m:[38;5;231m public DataHandler {[0m[0m [38;5;4m│[0m
[38;5;4m────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m123[0m
[38;5;231m   [38;5;203mstatic[38;5;231m [38;5;203minline[38;5;231m [38;5;149mHandle[38;5;203m<[38;5;231mSmi[38;5;203m>[38;5;231m LoadSlow(Isolate[38;5;203m*[38;5;231m isolate);[0m                                            
[38;5;231m [0m                                                                                                   
[38;5;231m   [38;5;242m// Creates a Smi-handler for loading a field from fast object.[0m                                   
[48;5;52m   static inline Handle<Smi> LoadField(Isolate* isolate, FieldIndex field_index[48;5;124m);[0m[48;5;52m                   [0m
[48;5;22;38;5;231m   [38;5;203mstatic[38;5;231m [38;5;203minline[38;5;231m [38;5;149mHandle[38;5;203m<[38;5;231mSmi[38;5;203m>[38;5;231m LoadField(Isolate[38;5;203m*[38;5;231m isolate, FieldIndex field_index[48;5;28m,[0m[48;5;22m                    [0m
[48;5;22;38;5;231m                                       Representation representation);[0m[48;5;22m                              [0m
[38;5;231m [0m                                                                                                   
[38;5;231m   [38;5;242m// Creates a Smi-handler for loading a cached constant from fast[0m                                 
[38;5;231m   [38;5;242m// prototype object.[0m                                                                             

[38;5;4m[38;5;4msrc/ic/ic.cc[0m[0m
[38;5;4m───────────────────────────────────────────────────────────────────────────────────────────────────[0m
[38;5;4m─────────────────────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m [38;5;149mHandle[38;5;231m<Object> [38;5;149mLoadIC::ComputeHandler[38;5;231m(LookupIterator[38;5;203m*[38;5;231m [38;5;208mlookup[38;5;231m) {[0m[0m [38;5;4m│[0m
[38;5;4m─────────────────────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m845[0m
[38;5;231m       [38;5;203mif[38;5;231m (Accessors::IsJSObjectFieldAccessor(isolate(), map, lookup->name(),[0m                       
[38;5;231m                                              [38;5;203m&[38;5;231mindex)) {[0m                                            
[38;5;231m         TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);[0m                                        
[48;5;52m         return LoadHandler::LoadField(isolate(), index[48;5;124m);[0m[48;5;52m                                           [0m
[48;5;22;38;5;231m         [38;5;203mreturn[38;5;231m LoadHandler::LoadField(isolate(), index[48;5;28m,[0m[48;5;22m                                            [0m
[48;5;22;38;5;231m                                       lookup->representation());[0m[48;5;22m                                   [0m
[38;5;231m       }[0m                                                                                            
[38;5;231m       [38;5;203mif[38;5;231m (holder->IsJSModuleNamespace()) {[0m                                                         
[38;5;231m         Handle<ObjectHashTable> exports([0m                                                           
[38;5;4m─────────────────────────────────────────────────────────────────[0m[38;5;4m┐[0m
[38;5;4m[38;5;231m [38;5;149mHandle[38;5;231m<Object> [38;5;149mLoadIC::ComputeHandler[38;5;231m(LookupIterator[38;5;203m*[38;5;231m [38;5;208mlookup[38;5;231m) {[0m[0m [38;5;4m│[0m
[38;5;4m─────────────────────────────────────────────────────────────────[0m[38;5;4m┘[0m
[38;5;4m987[0m
[38;5;231m       } [38;5;203melse[38;5;231m {[0m                                                                                     
[38;5;231m         DCHECK_EQ([38;5;141mkField[38;5;231m, lookup->property_details().location());[0m                                  
[38;5;231m         FieldIndex field [38;5;203m=[38;5;231m lookup->GetFieldIndex();[0m                                                
[48;5;52m         smi_handler = LoadHandler::LoadField(isolate(), field);[0m[48;5;52m                                    [0m
[48;5;22;38;5;231m         smi_handler [38;5;203m=[38;5;231m[0m[48;5;22m                                                                              [0m
[48;5;22;38;5;231m             LoadHandler::LoadField(isolate(), field, lookup->representation());[0m[48;5;22m                    [0m
[38;5;231m         TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);[0m                                        
[38;5;231m         [38;5;203mif[38;5;231m (holder_is_lookup_start_object) [38;5;203mreturn[38;5;231m smi_handler;[0m                                     
[38;5;231m         TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldFromPrototypeDH);[0m                           
