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
    // res.write
    // res.tryEnd

    /* Takes string or arraybuffer, returns this */
    template <bool SSL>
    static void res_end(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);
        getHttpResponse<SSL>(args)->end(std::string_view(data.getData(), data.getLength()));

        args.GetReturnValue().Set(args.Holder());
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
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader<SSL>));

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
