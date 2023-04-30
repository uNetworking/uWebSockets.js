/*
 * Authored by Alex Hultman, 2018-2020.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "App.h"
#include "Utilities.h"

#include <v8.h>
using namespace v8;

/* This one is the same for SSL and non-SSL */
struct HttpRequestWrapper {

    /* Unwraps the HttpRequest from V8 object */
    template <int QUIC>
    static inline constexpr decltype(auto) getHttpRequest(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        /* Thow on deleted request */
        auto *req = (uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!req) {
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "Using uWS.HttpRequest / uWS.Http3Request past its request handler return is forbidden (it is stack allocated).", NewStringType::kNormal).ToLocalChecked())));
        }

        if constexpr (QUIC) {
            return (uWS::Http3Request *) req;
        } else {
            return req;
        }
    }

    /* Takes function of string, string. Returns this (doesn't really but should) */
    template <int QUIC>
    static void req_forEach(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            Local<Function> cb = Local<Function>::Cast(args[0]);

            for (auto p : *req) {
                Local<Value> argv[] = {String::NewFromUtf8(isolate, p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked(),
                                       String::NewFromUtf8(isolate, p.second.data(), NewStringType::kNormal, p.second.length()).ToLocalChecked()};
                /* This one is also called from JS so no need for CallJS */
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv).IsEmpty();
            }
        }
    }

    /* Takes int, returns string (must be in bounds) */
    template <int QUIC>
    static void req_getParameter(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            int index = args[0]->Uint32Value(isolate->GetCurrentContext()).ToChecked();
            std::string_view parameter = req->getParameter(index);

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, parameter.data(), NewStringType::kNormal, parameter.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    static void req_getUrl(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view url = req->getUrl();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, url.data(), NewStringType::kNormal, url.length()).ToLocalChecked());
        }
    }

    /* Takes String, returns String */
    template <int QUIC>
    static void req_getHeader(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            std::string_view header = req->getHeader(data.getString());

            /* We want latin1 here */
            args.GetReturnValue().Set(String::NewFromOneByte(isolate, (const uint8_t *)header.data(), NewStringType::kNormal, header.length()).ToLocalChecked());
        }
    }

    /* Takes boolean, returns this */
    template <int QUIC>
    static void req_setYield(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            bool yield = args[0]->BooleanValue(isolate);
            req->setYield(yield);

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    static void req_getMethod(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view method = req->getMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), NewStringType::kNormal, method.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    static void req_getCaseSensitiveMethod(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view method = req->getCaseSensitiveMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), NewStringType::kNormal, method.length()).ToLocalChecked());
        }
    }

    template <int QUIC>
    static void req_getQuery(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view query;

            /* Do we have a key argument? */
            if (args.Length() == 1) {
                NativeString keyString(isolate, args[0]);
                if (keyString.isInvalid(args)) {
                    return;
                }

                query = req->getQuery(keyString.getString());
            } else {
                query = req->getQuery();
            }
            
            /* If we have nullptr as data, it's not simply an empty string */
            if (query.data() == nullptr) {
                return;
            }

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, query.data(), NewStringType::kNormal, query.length()).ToLocalChecked());
        }
    }

    /* Returns a clonable object wrapping an HttpRequest */
    template <int QUIC>
    static Local<Object> init(Isolate *isolate) {
        /* We do clone every request object, we could share them, they are illegal to use outside the function anyways */
        Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
        reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, QUIC ? "uWS.Http3Request" : "uWS.HttpRequest", NewStringType::kNormal).ToLocalChecked());
        reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getHeader<QUIC>));
        
        if constexpr (!QUIC) {
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getParameter", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getParameter<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getUrl", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getUrl<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getMethod", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getMethod<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getCaseSensitiveMethod", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getCaseSensitiveMethod<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getQuery", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_getQuery<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "forEach", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_forEach<QUIC>));
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "setYield", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, req_setYield<QUIC>));
        }


        /* Create the template */
        Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return reqObjectLocal;
    }
};
