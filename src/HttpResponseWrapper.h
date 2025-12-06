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

thread_local int insideCorkCallback = 0;

/* PROTOCOL is 0 = TCP, 1 = TLS, 2 = QUIC, 3 = CACHE */

struct HttpResponseWrapper {

    static void assumeCorked() {
        if (!insideCorkCallback) {
            std::cerr << "Warning: uWS.HttpResponse writes must be made from within a corked callback. See documentation for uWS.HttpResponse.cork and consult the user manual." << std::endl;
        }
    }

    template <int PROTOCOL>
    static inline constexpr decltype(auto) getHttpResponse(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        void *res = args.This()->GetAlignedPointerFromInternalField(0);
        if (!res) {
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "uWS.HttpResponse must not be accessed after uWS.HttpResponse.onAborted callback, or after a successful response. See documentation for uWS.HttpResponse and consult the user manual.", NewStringType::kNormal).ToLocalChecked())));
        }

        if constexpr (PROTOCOL == 2) {
            return (uWS::Http3Response *) res;
        } else if constexpr (PROTOCOL == 3) {
            //return (uWS::CachingHttpResponse *) res; // is correct
            return (uWS::HttpResponse<PROTOCOL != 0> *) res; // not correct
        } else {
            return (uWS::HttpResponse<PROTOCOL != 0> *) res;
        }
    }

    /* Marks this JS object invalid */
    static inline void invalidateResObject(const FunctionCallbackInfo<Value> &args) {
        args.This()->SetAlignedPointerInInternalField(0, nullptr);
    }

    /* Takes nothing, returns this */
    template <int SSL>
    static void res_pause(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            res->pause();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns this */
    template <int SSL>
    static void res_resume(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            res->resume();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, kills the connection */
    template <int SSL>
    static void res_close(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            invalidateResObject(args);
            res->close();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes function of data and isLast. Expects nothing from callback, returns this */
    template <int SSL>
    static void res_onData(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onData([p = std::move(p), isolate](std::string_view data, bool last) {
                HandleScope hs(isolate);

                Local<ArrayBuffer> dataArrayBuffer = ArrayBuffer_New(isolate, (void *) data.data(), data.length());

                Local<Value> argv[] = {dataArrayBuffer, Boolean::New(isolate, last)};
                CallJS(isolate, Local<Function>::New(isolate, p), 2, argv);

                dataArrayBuffer->Detach();
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns nothing. Cb wants nothing returned. */
    template <int SSL>
    static void res_onAborted(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            /* This is how we capture res (C++ this in invocation of this function) */
            UniquePersistent<Object> resObject(isolate, args.This());

            res->onAborted([p = std::move(p), resObject = std::move(resObject), isolate]() {
                HandleScope hs(isolate);

                /* Mark this resObject invalid */
                Local<Object>::New(isolate, resObject)->SetAlignedPointerInInternalField(0, nullptr);

                CallJS(isolate, Local<Function>::New(isolate, p), 0, nullptr);
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    static void res_getRemoteAddress(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    static void res_getRemoteAddressAsText(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns integer */
    template <int SSL>
    static void res_getRemotePort(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            unsigned int port = res->getRemotePort();

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    static void res_getProxiedRemoteAddress(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    static void res_getProxiedRemoteAddressAsText(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns number */
    template <int SSL>
    static void res_getProxiedRemotePort(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            unsigned int port = res->getProxiedRemotePort();

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    template <int PROTOCOL>
    static void res_getX509Certificate(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            void* sslHandle = res->getNativeHandle();
            SSL* ssl = static_cast<SSL*>(sslHandle);
            std::string x509cert = extractX509PemCertificate(ssl);
            args.GetReturnValue().Set(String::NewFromUtf8(isolate, x509cert.c_str(), NewStringType::kNormal).ToLocalChecked());
        }
    }

    /* Returns the current write offset */
    template <int SSL>
    static void res_getWriteOffset(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            args.GetReturnValue().Set(Number::New(isolate, getHttpResponse<SSL>(args)->getWriteOffset()));
        }
    }

    /* Takes function of bool(int), returns this */
    template <int SSL>
    static void res_onWritable(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onWritable([p = std::move(p), isolate](size_t offset) -> bool {
                HandleScope hs(isolate);

                Local<Value> argv[] = {Number::New(isolate, offset)};

                /* We should check if this is really here! */
                MaybeLocal<Value> maybeBoolean = CallJS(isolate, Local<Function>::New(isolate, p), 1, argv);
                if (maybeBoolean.IsEmpty()) {
                    std::cerr << "Warning: uWS.HttpResponse.onWritable callback should return Boolean. See documentation for uWS.HttpResponse.onWritable and consult the user manual." << std::endl;
                    /* The default should be true, as it only adds a potential extra send, rather than erroneously avoid it */
                    return true;
                }

                return maybeBoolean.ToLocalChecked()->BooleanValue(isolate);
                /* How important is this return? */
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes string or arraybuffer, returns this */
    template <int SSL>
    static void res_writeStatus(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
            if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            assumeCorked();
            res->writeStatus(data.getString());

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes number, bool */
    template <int SSL>
    static void res_endWithoutBody(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::optional<size_t> reportedContentLength;
            if (args.Length() >= 1) {
                reportedContentLength = (size_t) args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).ToChecked();
            }

            bool closeConnection = false;
            if (args.Length() >= 2) {
                closeConnection = args[1]->BooleanValue(args.GetIsolate());
            }

            invalidateResObject(args);
            assumeCorked();
            res->endWithoutBody(reportedContentLength, closeConnection);

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes string or arraybuffer, returns this */
    template <int PROTOCOL>
    static void res_end(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            bool closeConnection = false;
            if (args.Length() >= 2) {
                closeConnection = args[1]->BooleanValue(args.GetIsolate());
            }

            invalidateResObject(args);

            assumeCorked();
            res->end(data.getString(), closeConnection);

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes data and optionally totalLength, returns true for success, false for backpressure */
    template <int PROTOCOL>
    static void res_tryEnd(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            size_t totalSize = 0;
            if (args.Length() > 1) {
                totalSize = (size_t) args[1]->NumberValue(isolate->GetCurrentContext()).ToChecked();
            }

            assumeCorked();
            auto [ok, hasResponded] = res->tryEnd(data.getString(), totalSize);

            /* Invalidate this object if we responded completely */
            if (hasResponded) {
                invalidateResObject(args);
            }

            /* This is a quick fix, it will need updating in µWS later on */
            Local<Array> array = Array::New(isolate, 2);
            array->Set(isolate->GetCurrentContext(), 0, Boolean::New(isolate, ok)).ToChecked();
            array->Set(isolate->GetCurrentContext(), 1, Boolean::New(isolate, hasResponded)).ToChecked();

            args.GetReturnValue().Set(array);
        }
    }

    /* Takes data, returns true for success, false for backpressure */
    template <int PROTOCOL>
    static void res_write(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }
            assumeCorked();
            bool ok = res->write(data.getString());

            args.GetReturnValue().Set(Boolean::New(isolate, ok));
        }
    }

    /* Takes key, value. Returns this */
    template <int PROTOCOL>
    static void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString header(args.GetIsolate(), args[0]);
            if (header.isInvalid(args)) {
                return;
            }
            NativeString value(args.GetIsolate(), args[1]);
            if (value.isInvalid(args)) {
                return;
            }
            assumeCorked();
            res->writeHeader(header.getString(), value.getString());

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes function, returns this */
    template <int SSL>
    static void res_cork(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {

            res->cork([cb = Local<Function>::Cast(args[0]), isolate]() {
                insideCorkCallback++;
                /* This one is called from JS so we don't need CallJS */
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 0, nullptr).IsEmpty();
                insideCorkCallback--;
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes UserData, secKey, secProtocol, secExtensions, context. Returns nothing */
    template <int SSL>
    static void res_upgrade(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* We require exactly 5 arguments */
            if (args.Length() != 5) {
                return;
            }

            NativeString secWebSocketKey(args.GetIsolate(), args[1]);
            if (secWebSocketKey.isInvalid(args)) {
                return;
            }

            NativeString secWebSocketProtocol(args.GetIsolate(), args[2]);
            if (secWebSocketProtocol.isInvalid(args)) {
                return;
            }

            NativeString secWebSocketExtensions(args.GetIsolate(), args[3]);
            if (secWebSocketExtensions.isInvalid(args)) {
                return;
            }

            auto *context = (struct us_socket_context_t *) Local<External>::Cast(args[4])->Value();

            invalidateResObject(args);

            /* This releases on return */
            UniquePersistent<Object> userData;
            userData.Reset(isolate, Local<Object>::Cast(args[0]));

            /* Immediately calls open handler */
            assumeCorked();
            res->template upgrade<PerSocketData>({
                std::move(userData)
            }, secWebSocketKey.getString(), secWebSocketProtocol.getString(),
                secWebSocketExtensions.getString(), context);

            /* Nothing is returned */
        }
    }

    /* 0 = TCP, 1 = TLS, 2 = QUIC, 3 = CACHE */
    template <int SSL>
    static Local<Object> init(Isolate *isolate) {
        Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
        if (SSL == 1) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLHttpResponse", NewStringType::kNormal).ToLocalChecked());
        } else if (SSL == 0) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse", NewStringType::kNormal).ToLocalChecked());
        } else if (SSL == 2) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.Http3Response", NewStringType::kNormal).ToLocalChecked());
        } else if (SSL == 3) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.CachedHttpResponse", NewStringType::kNormal).ToLocalChecked());
        }
        resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_end<SSL>));
        
        /* Cache has almost nothing wrapped yet */
        if constexpr (SSL != 3) {
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeStatus", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_writeStatus<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "endWithoutBody", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_endWithoutBody<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "tryEnd", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_tryEnd<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "write", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_write<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_writeHeader<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_close<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onWritable", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onWritable<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onAborted", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onAborted<SSL>));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onData", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onData<SSL>));

            /* QUIC has a lot of functions unimplemented */
            if constexpr (SSL != 2) {
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getWriteOffset", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getWriteOffset<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getRemoteAddress<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "cork", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_cork<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "collect", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_cork<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "upgrade", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_upgrade<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddressAsText", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getRemoteAddressAsText<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemotePort", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getRemotePort<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getProxiedRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getProxiedRemoteAddress<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getProxiedRemoteAddressAsText", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getProxiedRemoteAddressAsText<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getProxiedRemotePort", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getProxiedRemotePort<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "pause", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_pause<SSL>));
                resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "resume", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_resume<SSL>));
            }
        }

        if constexpr (SSL == 1) {
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getX509Certificate", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getX509Certificate<SSL>));
        }
        
        /* Create our template */
        Local<Object> resObjectLocal = resTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return resObjectLocal;
    }
};
