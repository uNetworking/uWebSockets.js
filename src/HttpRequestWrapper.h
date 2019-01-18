#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

/* This one is the same for SSL and non-SSL */
struct HttpRequestWrapper {
    static Persistent<Object> reqTemplate;

    static inline uWS::HttpRequest *getHttpRequest(const FunctionCallbackInfo<Value> &args) {
        /* Thow on deleted request */
        auto *req = (uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!req) {
            args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "Using uWS.HttpRequest past its request handler return is forbidden (it is stack allocated).")));
        }
        return req;
    }

    /* Takes int, returns string (must be in bounds) */
    static void req_getParameter(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            int index = args[0]->Uint32Value();
            std::string_view parameter = req->getParameter(index);

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, parameter.data(), v8::String::kNormalString, parameter.length()));
        }
    }

    /* Takes nothing, returns string */
    static void req_getUrl(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view url = req->getUrl();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, url.data(), v8::String::kNormalString, url.length()));
        }
    }

    /* Takes String, returns String */
    static void req_getHeader(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            std::string_view header = req->getHeader(data.getString());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
        }
    }

    /* Takes nothing, returns string */
    static void req_getMethod(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view method = req->getMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), v8::String::kNormalString, method.length()));
        }
    }

    static void req_getQuery(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view query = req->getQuery();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, query.data(), v8::String::kNormalString, query.length()));
        }
    }

    static void initReqTemplate() {
        /* We do clone every request object, we could share them, they are illegal to use outside the function anyways */
        Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
        reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
        reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getParameter"), FunctionTemplate::New(isolate, req_getParameter));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getUrl"), FunctionTemplate::New(isolate, req_getUrl));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getMethod"), FunctionTemplate::New(isolate, req_getMethod));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getQuery"), FunctionTemplate::New(isolate, req_getQuery));

        /* Create the template */
        Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        reqTemplate.Reset(isolate, reqObjectLocal);
    }

    static Local<Object> getReqInstance() {
        return Local<Object>::New(isolate, reqTemplate)->Clone();
    }
};

Persistent<Object> HttpRequestWrapper::reqTemplate;
