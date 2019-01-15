#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

struct HttpResponseWrapper {
    static Persistent<Object> resTemplate[2];

    template <bool SSL>
    static inline uWS::HttpResponse<SSL> *getHttpResponse(const FunctionCallbackInfo<Value> &args) {
        return (uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0);
    }

    // res.onData(JS function)
    // res.onAborted

    /* Takes function of bool(int), returns this */
    template <bool SSL>
    static void res_onWritable(const FunctionCallbackInfo<Value> &args) {
        /* This thing perfectly fits in with unique_function, and will Reset on destructor */
        UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

        getHttpResponse<SSL>(args)->onWritable([p = std::move(p)](int offset) {
            HandleScope hs(isolate);

            Local<Value> argv[] = {Integer::NewFromUnsigned(isolate, offset)};
            return Local<Function>::New(isolate, p)->Call(isolate->GetCurrentContext()->Global(), 1, argv)->BooleanValue();
        });

        args.GetReturnValue().Set(args.Holder());
    }

    /* Takes string or arraybuffer, returns this */
    template <bool SSL>
    static void res_writeStatus(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);
        getHttpResponse<SSL>(args)->writeStatus(std::string_view(data.getData(), data.getLength()));

        args.GetReturnValue().Set(args.Holder());
    }

    /* Takes string or arraybuffer, returns this */
    template <bool SSL>
    static void res_end(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);
        getHttpResponse<SSL>(args)->end(std::string_view(data.getData(), data.getLength()));

        args.GetReturnValue().Set(args.Holder());
    }

    /* Takes data and optionally totalLength, returns true for success, false for backpressure */
    template <bool SSL>
    static void res_tryEnd(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);

        int totalSize = 0;
        if (args.Length() > 1) {
            totalSize = args[1]->Uint32Value();
        }

        bool ok = getHttpResponse<SSL>(args)->tryEnd(std::string_view(data.getData(), data.getLength()), totalSize);

        args.GetReturnValue().Set(Boolean::New(isolate, ok));
    }

    /* Takes data, returns true for success, false for backpressure */
    template <bool SSL>
    static void res_write(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);
        bool ok = getHttpResponse<SSL>(args)->write(std::string_view(data.getData(), data.getLength()));

        args.GetReturnValue().Set(Boolean::New(isolate, ok));
    }

    /* Takes key, value. Returns this */
    template <bool SSL>
    static void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
        NativeString header(args.GetIsolate(), args[0]);
        NativeString value(args.GetIsolate(), args[1]);
        getHttpResponse<SSL>(args)->writeHeader(std::string_view(header.getData(), header.getLength()),
                                                std::string_view(value.getData(), value.getLength()));

        args.GetReturnValue().Set(args.Holder());
    }

    template <bool SSL>
    static void initResTemplate() {
        Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
        if (SSL) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLHttpResponse"));
        } else {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
        }
        resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeStatus"), FunctionTemplate::New(isolate, res_writeStatus<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "tryEnd"), FunctionTemplate::New(isolate, res_tryEnd<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "write"), FunctionTemplate::New(isolate, res_write<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader<SSL>));

        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onWritable"), FunctionTemplate::New(isolate, res_onWritable<SSL>));

        /* Create our template */
        Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        resTemplate[SSL].Reset(isolate, resObjectLocal);
    }

    template <class APP>
    static Local<Object> getResInstance() {
        return Local<Object>::New(isolate, resTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
    }
};

Persistent<Object> HttpResponseWrapper::resTemplate[2];
