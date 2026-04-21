/*
 * Authored by Alex Hultman, 2018-2026.
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

#include "Utilities.h"

/* This one is the same for SSL and non-SSL */
namespace HttpRequestWrapper {

    /* Unwraps the HttpRequest from V8 object */
    template <int QUIC>
    inline constexpr decltype(auto) getHttpRequest(args_t args) {
        Isolate *isolate = args.GetIsolate();
        /* Thow on deleted request */
        auto *req = (uWS::HttpRequest *) args.This()->GetAlignedPointerFromInternalField(0);
        if (!req) {
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "uWS.HttpRequest must not be accessed after await or route handler return. See documentation for uWS.HttpRequest and consult the user manual.", NewStringType::kNormal).ToLocalChecked())));
        }

        if constexpr (QUIC) {
            return (uWS::Http3Request *) req;
        } else {
            return req;
        }
    }

    /* Takes function of string, string. Returns this (doesn't really but should) */
    template <int QUIC>
    void req_forEach(args_t args) {
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

    /* Takes int or string, returns string (must be in bounds) */
    template <int QUIC>
    void req_getParameter(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {

            /* Either an integer index or string name */
            std::string_view parameter;
            if (args[0]->IsNumber()) {
                int index = args[0]->Uint32Value(isolate->GetCurrentContext()).ToChecked();
                parameter = req->getParameter(index);
            } else {
                NativeString<true> data(args.GetIsolate(), args[0]);
                if (data.isInvalid(args)) {
                    return;
                }
                parameter = req->getParameter(data.getString());
            }

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, parameter.data(), NewStringType::kNormal, parameter.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    void req_getUrl(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view url = req->getUrl();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, url.data(), NewStringType::kNormal, url.length()).ToLocalChecked());
        }
    }

    /* Takes String, returns String */
    template <int QUIC>
    void req_getHeader(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            NativeString<true> data(args.GetIsolate(), args[0]);
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
    void req_setYield(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            bool yield = args[0]->BooleanValue(isolate);
            req->setYield(yield);

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    void req_getMethod(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view method = req->getMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), NewStringType::kNormal, method.length()).ToLocalChecked());
        }
    }

    /* Takes nothing, returns string */
    template <int QUIC>
    void req_getCaseSensitiveMethod(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view method = req->getCaseSensitiveMethod();

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, method.data(), NewStringType::kNormal, method.length()).ToLocalChecked());
        }
    }

    template <int QUIC>
    void req_getQuery(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *req = getHttpRequest<QUIC>(args);
        if (req) {
            std::string_view query;

            /* Do we have a key argument? */
            if (args.Length() == 1) {
                NativeString<true> keyString(isolate, args[0]);
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
    Local<Object> init(Isolate *isolate) {
        /* We do clone every request object, we could share them, they are illegal to use outside the function anyways */
        Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
        reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, QUIC == 2 ? "uWS.Http3Request" : "uWS.HttpRequest", NewStringType::kNormal).ToLocalChecked());
        reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* helper */
          auto regFn = [reqTemplateObject = reqTemplateLocal->PrototypeTemplate(), isolate]<size_t N>(
            const char (&str)[N],
            void(*cb)(args_t)
          ){
            reqTemplateObject->Set(
              String::NewFromUtf8(isolate, str, NewStringType::kNormal, N-1).ToLocalChecked(),
              FunctionTemplate::New(isolate, cb)
            );
          };

        /* Register our functions */
        regFn("getHeader", req_getHeader<QUIC>);
        
        if constexpr (QUIC == 0) {
          regFn("getParameter", req_getParameter<QUIC>);
          regFn("getUrl", req_getUrl<QUIC>);
          regFn("getMethod", req_getMethod<QUIC>);
          regFn("getCaseSensitiveMethod", req_getCaseSensitiveMethod<QUIC>);
          regFn("getQuery", req_getQuery<QUIC>);
          regFn("forEach", req_forEach<QUIC>);
          regFn("setYield", req_setYield<QUIC>);
        }


        /* Create the template */
        Local<Context> context = isolate->GetCurrentContext();
        Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction(context).ToLocalChecked()->NewInstance(context).ToLocalChecked();

        return reqObjectLocal;
    }
};
