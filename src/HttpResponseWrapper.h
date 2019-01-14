// res.onData(JS function)
// res.write
// res.tryEnd

// this whole template thing could be one struct with members to order tihngs better
Persistent<Object> resTemplate[2];

template <bool SSL>
void res_end(const FunctionCallbackInfo<Value> &args) {
    NativeString data(args.GetIsolate(), args[0]);
    ((uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0))->end(std::string_view(data.getData(), data.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

template <bool SSL>
void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
    NativeString header(args.GetIsolate(), args[0]);
    NativeString value(args.GetIsolate(), args[1]);
    ((uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0))->writeHeader(std::string_view(header.getData(), header.getLength()),
                                                                                                     std::string_view(value.getData(), value.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

template <bool SSL>
void initResTemplate() {
    Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
    if (SSL) {
        resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLHttpResponse"));
    } else {
        resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
    }
    resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end<SSL>));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader<SSL>));

    Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    resTemplate[SSL].Reset(isolate, resObjectLocal);
}

template <class APP>
Local<Object> getResInstance() {
    return Local<Object>::New(isolate, resTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
}
