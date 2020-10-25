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

struct HttpResponseWrapper {

    template <bool SSL>
    static inline uWS::HttpResponse<SSL> *getHttpResponse(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = (uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!res) {
            args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "Invalid access of discarded (invalid, deleted) uWS.HttpResponse/SSLHttpResponse.", NewStringType::kNormal).ToLocalChecked()));
        }
        return res;
    }

    /* Marks this JS object invalid */
    static inline void invalidateResObject(const FunctionCallbackInfo<Value> &args) {
        args.Holder()->SetAlignedPointerInInternalField(0, nullptr);
    }

    /* Takes nothing, kills the connection */
    template <bool SSL>
    static void res_close(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            invalidateResObject(args);
            res->close();
            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes function of data and isLast. Expects nothing from callback, returns this */
    template <bool SSL>
    static void res_onData(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onData([p = std::move(p), isolate](std::string_view data, bool last) {
                HandleScope hs(isolate);

                Local<ArrayBuffer> dataArrayBuffer = ArrayBuffer::New(isolate, (void *) data.data(), data.length());

                Local<Value> argv[] = {dataArrayBuffer, Boolean::New(isolate, last)};
                CallJS(isolate, Local<Function>::New(isolate, p), 2, argv);

                NeuterArrayBuffer(dataArrayBuffer);
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes nothing, returns nothing. Cb wants nothing returned. */
    template <bool SSL>
    static void res_onAborted(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            /* This is how we capture res (C++ this in invocation of this function) */
            UniquePersistent<Object> resObject(isolate, args.Holder());

            res->onAborted([p = std::move(p), resObject = std::move(resObject), isolate]() {
                HandleScope hs(isolate);

                /* Mark this resObject invalid */
                Local<Object>::New(isolate, resObject)->SetAlignedPointerInInternalField(0, nullptr);

                CallJS(isolate, Local<Function>::New(isolate, p), 0, nullptr);
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <bool SSL>
    static void res_getRemoteAddress(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddress();

            /* Todo: we need to pass a copy here */
            args.GetReturnValue().Set(ArrayBuffer::New(isolate, (void *) ip.data(), ip.length()/*, ArrayBufferCreationMode::kInternalized*/));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <bool SSL>
    static void res_getRemoteAddressAsText(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddressAsText();

            /* Todo: we need to pass a copy here */
            args.GetReturnValue().Set(ArrayBuffer::New(isolate, (void *) ip.data(), ip.length()/*, ArrayBufferCreationMode::kInternalized*/));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <bool SSL>
    static void res_getProxiedRemoteAddress(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddress();

            /* Todo: we need to pass a copy here */
            args.GetReturnValue().Set(ArrayBuffer::New(isolate, (void *) ip.data(), ip.length()/*, ArrayBufferCreationMode::kInternalized*/));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <bool SSL>
    static void res_getProxiedRemoteAddressAsText(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddressAsText();

            /* Todo: we need to pass a copy here */
            args.GetReturnValue().Set(ArrayBuffer::New(isolate, (void *) ip.data(), ip.length()/*, ArrayBufferCreationMode::kInternalized*/));
        }
    }

    /* Returns the current write offset */
    template <bool SSL>
    static void res_getWriteOffset(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            args.GetReturnValue().Set(Number::New(isolate, getHttpResponse<SSL>(args)->getWriteOffset()));
        }
    }

    /* Takes function of bool(int), returns this */
    template <bool SSL>
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
                    std::cerr << "ERROR! onWritable must return a boolean value according to documentation!" << std::endl;
                    exit(-1);
                }

                return BooleanValue(isolate, maybeBoolean.ToLocalChecked());
                /* How important is this return? */
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes string or arraybuffer, returns this */
    template <bool SSL>
    static void res_writeStatus(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
            if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }
            res->writeStatus(data.getString());

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes string or arraybuffer, returns this */
    template <bool SSL>
    static void res_end(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }
            invalidateResObject(args);
            res->end(data.getString());

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes data and optionally totalLength, returns true for success, false for backpressure */
    template <bool SSL>
    static void res_tryEnd(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            size_t totalSize = 0;
            if (args.Length() > 1) {
                totalSize = (size_t) args[1]->NumberValue(isolate->GetCurrentContext()).ToChecked();
            }

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
    template <bool SSL>
    static void res_write(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }
            bool ok = res->write(data.getString());

            args.GetReturnValue().Set(Boolean::New(isolate, ok));
        }
    }

    /* Takes key, value. Returns this */
    template <bool SSL>
    static void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            NativeString header(args.GetIsolate(), args[0]);
            if (header.isInvalid(args)) {
                return;
            }
            NativeString value(args.GetIsolate(), args[1]);
            if (value.isInvalid(args)) {
                return;
            }
            res->writeHeader(header.getString(), value.getString());

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes function, returns this */
    template <bool SSL>
    static void res_cork(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {

            res->cork([cb = Local<Function>::Cast(args[0]), isolate]() {
                /* This one is called from JS so we don't need CallJS */
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 0, nullptr).IsEmpty();
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes UserData, secKey, secProtocol, secExtensions, context. Returns nothing */
    template <bool SSL>
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
            res->template upgrade<PerSocketData>({
                std::move(userData)
            }, secWebSocketKey.getString(), secWebSocketProtocol.getString(),
                secWebSocketExtensions.getString(), context);

            /* Nothing is returned */
        }
    }

    template <bool SSL>
    static Local<Object> init(Isolate *isolate) {
        Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
        if (SSL) {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLHttpResponse", NewStringType::kNormal).ToLocalChecked());
        } else {
            resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse", NewStringType::kNormal).ToLocalChecked());
        }
        resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeStatus", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_writeStatus<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_end<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "tryEnd", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_tryEnd<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "write", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_write<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_writeHeader<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_close<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getWriteOffset", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getWriteOffset<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onWritable", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onWritable<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onAborted", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onAborted<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onData", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_onData<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getRemoteAddress<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "cork", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_cork<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "upgrade", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_upgrade<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddressAsText", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getRemoteAddressAsText<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getProxiedRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getProxiedRemoteAddress<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getProxiedRemoteAddressAsText", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, res_getProxiedRemoteAddressAsText<SSL>));

        /* Create our template */
        Local<Object> resObjectLocal = resTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return resObjectLocal;
    }
};
