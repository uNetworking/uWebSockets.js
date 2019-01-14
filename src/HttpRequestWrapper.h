// req.getParam(int)
// req.getUrl()
// req.onAbort ?

/* The only thing this req needs is getHeader and similar, getParameter, getUrl and so on */
Persistent<Object> reqTemplate;

void req_getHeader(const FunctionCallbackInfo<Value> &args) {
    NativeString data(args.GetIsolate(), args[0]);
    char *buf = data.getData(); int length = data.getLength();

    std::string_view header = ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0))->getHeader(std::string_view(buf, length));

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
}

void initReqTemplate() {
    /*reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
    reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);*/

    // Request template (do we need to clone this?)
    Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
    reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
    reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));

    Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    reqTemplate.Reset(isolate, reqObjectLocal);
}
