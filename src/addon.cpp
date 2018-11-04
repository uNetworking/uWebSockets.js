/* We are only allowed to depend on µWS and V8 in this layer. */
#include "App.h"
#include <v8.h>
using namespace v8;
Isolate *isolate;
Persistent<Object> resTemplate;
Persistent<Object> reqTemplate;

#include <iostream>
#include <vector>
std::vector<Persistent<Function, CopyablePersistentTraits<Function>>> nextTickQueue;

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
public:
    NativeString(Isolate *isolate, const Local<Value> &value) {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            utf8Value = new (utf8ValueMemory) String::Utf8Value(isolate, value);
            data = (**utf8Value);
            length = utf8Value->length();
        } else if (value->IsTypedArray()) {
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            ArrayBuffer::Contents contents = arrayBufferView->Buffer()->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            ArrayBuffer::Contents contents = arrayBuffer->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else {
            static char empty[] = "";
            data = empty;
            length = 0;
        }
    }

    char *getData() {return data;}
    size_t getLength() {return length;}
    ~NativeString() {
        if (utf8Value) {
            utf8Value->~Utf8Value();
        }
    }
};

void res_end(const FunctionCallbackInfo<Value> &args) {

    // you might want to do extra work here to swap to tryEnd if passed a Buffer?
    // or always use tryEnd and simply grab the object as persistent?

    NativeString data(args.GetIsolate(), args[0]);
    ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->end(std::string_view(data.getData(), data.getLength()));

    // Return this
    args.GetReturnValue().Set(args.Holder());
}

void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
    // get string
    NativeString header(args.GetIsolate(), args[0]);
    NativeString value(args.GetIsolate(), args[1]);

    ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->writeHeader(std::string_view(header.getData(), header.getLength()), std::string_view(value.getData(), value.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

void req_getHeader(const FunctionCallbackInfo<Value> &args) {
    // get string
    NativeString data(args.GetIsolate(), args[0]);
    char *buf = data.getData(); int length = data.getLength();

    std::string_view header = ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0))->getHeader(std::string_view(buf, length));

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
}

void uWS_App_get(const FunctionCallbackInfo<Value> &args) {
    uWS::App *app = (uWS::App *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString nativeString(args.GetIsolate(), args[0]);

    Persistent<Function> *pf = new Persistent<Function>();
    pf->Reset(args.GetIsolate(), Local<Function>::Cast(args[1]));

    //Persistent<Function, CopyablePersistentTraits<Function>> p(isolate, Local<Function>::Cast(args[1]));

    app->get(std::string(nativeString.getData(), nativeString.getLength()), [pf](auto *res, auto *req) {
        HandleScope hs(isolate);

        Local<Object> resObject = Local<Object>::New(isolate, resTemplate)->Clone();
        resObject->SetAlignedPointerInInternalField(0, res);

        Local<Object> reqObject = Local<Object>::New(isolate, reqTemplate)->Clone();
        reqObject->SetAlignedPointerInInternalField(0, req);

        Local<Value> argv[] = {resObject, reqObject};
        Local<Function>::New(isolate, *pf)->Call(isolate->GetCurrentContext()->Global(), 2, argv);
    });

    args.GetReturnValue().Set(args.Holder());
}

// port, callback?
void uWS_App_listen(const FunctionCallbackInfo<Value> &args) {
    uWS::App *app = (uWS::App *) args.Holder()->GetAlignedPointerFromInternalField(0);

    int port = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).ToChecked();

    app->listen(port, [&args](auto *token) {
        Local<Value> argv[] = {Boolean::New(isolate, token)};
        Local<Function>::Cast(args[1])->Call(isolate->GetCurrentContext()->Global(), 1, argv);
    });

    // Return this
    args.GetReturnValue().Set(args.Holder());
}

// we need to override process.nextTick to avoid horrible loss of performance by Node.js
void nextTick(const FunctionCallbackInfo<Value> &args) {
    nextTickQueue.push_back(Persistent<Function, CopyablePersistentTraits<Function>>(isolate, Local<Function>::Cast(args[0])));
}

// uWS.App() -> object
void uWS_App(const FunctionCallbackInfo<Value> &args) {
    // uWS.App
    Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);
    appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.App"));
    appTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    // Get
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "get"), FunctionTemplate::New(isolate, uWS_App_get));

    // Listen
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen"), FunctionTemplate::New(isolate, uWS_App_listen));

    // Instantiate and set intenal pointer
    Local<Object> localApp = appTemplate->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    // Delete this boy
    localApp->SetAlignedPointerInInternalField(0, new uWS::App());

    // Return an instance of this shit
    args.GetReturnValue().Set(localApp);
}

void emptyNextTickQueue(Isolate *isolate) {
    if (nextTickQueue.size()) {
        HandleScope hs(isolate);

        for (Persistent<Function, CopyablePersistentTraits<Function>> &f : nextTickQueue) {
            Local<Function>::New(isolate, f)->Call(isolate->GetCurrentContext()->Global(), 0, nullptr);
            f.Reset();
        }

        nextTickQueue.clear();
    }
}

void Main(Local<Object> exports) {

    isolate = exports->GetIsolate();

    /* Register our own nextTick handler */
    /* We should probably also do this in pre, just to be sure */
    uWS::Loop::defaultLoop()->setPostHandler([isolate](uWS::Loop *) {
        emptyNextTickQueue(isolate);
    });

    /*reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
    reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);*/

    /* uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "App"), FunctionTemplate::New(isolate, uWS_App)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "nextTick"), FunctionTemplate::New(isolate, nextTick)->GetFunction());


    // HttpResponse template
    Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
    resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
    resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader));

    Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    resTemplate.Reset(isolate, resObjectLocal);

    // Request template (do we need to clone this?)
    Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
    reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
    reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));

    Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    reqTemplate.Reset(isolate, reqObjectLocal);
}

/* This is required when building as a Node.js addon */
#ifndef ADDON_IS_HOST
#include <node.h>
NODE_MODULE(uWS, Main)
#endif
