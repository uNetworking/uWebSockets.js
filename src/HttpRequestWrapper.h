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
            args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "Using uWS.HttpRequest past its request handler return is forbidden (it is stack allocated).", NewStringType::kNormal).ToLocalChecked()));
        }
        return req;
    }

    /* Takes function of string, string. Returns this (doesn't really but should) */
    static void req_forEach(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            Local<Function> cb = Local<Function>::Cast(args[0]);

            for (auto p : *req) {
                Local<Value> argv[] = {String::NewFromUtf8(isolate, p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked(),
                                       String::NewFromUtf8(isolate, p.second.data(), NewStringType::kNormal, p.second.length()).ToLocalChecked()};
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv).IsEmpty();
            }
        }
    }

    /* Takes int, returns string (must be in bounds) */
    static void req_getParameter(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            int index = args[0]->Uint32Value(isolate->GetCurrentContext()).ToChecked();
            std::string_view parameter = req->getParameter(index);

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, parameter.data(), NewStringType::kNormal, parameter.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    static void req_getUrl(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view url = req->getUrl();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, url.data(), NewStringType::kNormal, url.length()).ToLocalChecked());
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

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), NewStringType::kNormal, header.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    static void req_getMethod(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view method = req->getMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), NewStringType::kNormal, method.length()).ToLocalChecked());
        }
    }

    static void req_getQuery(const FunctionCallbackInfo<Value> &args) {
        auto *req = getHttpRequest(args);
        if (req) {
            std::string_view query = req->getQuery();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, query.data(), NewStringType::kNormal, query.length()).ToLocalChecked());
        }
    }

    static void initReqTemplate() {
        /* We do clone every request object, we could share them, they are illegal to use outside the function anyways */
        Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
        reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest", NewStringType::kNormal).ToLocalChecked());
        reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getHeader));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getParameter", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getParameter));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getUrl", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getUrl));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getMethod", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getMethod));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getQuery", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getQuery));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "forEach", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_forEach));

        /* Create the template */
        Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        reqTemplate.Reset(isolate, reqObjectLocal);
    }

    static Local<Object> getReqInstance() {
        return Local<Object>::New(isolate, reqTemplate)->Clone();
    }
};

Persistent<Object> HttpRequestWrapper::reqTemplate;
