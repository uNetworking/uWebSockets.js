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

thread_local int insideCorkCallback = 0;

/* PROTOCOL is 0 = TCP, 1 = TLS, 2 = QUIC, 3 = CACHE */

namespace HttpResponseWrapper {

    void assumeCorked() {
        if (!insideCorkCallback) {
            std::cerr << "Warning: uWS.HttpResponse writes must be made from within a corked callback. See documentation for uWS.HttpResponse.cork and consult the user manual." << std::endl;
        }
    }

    template <int PROTOCOL>
    inline constexpr decltype(auto) getHttpResponse(args_t args) {
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
    inline void invalidateResObject(args_t args) {
        args.This()->SetAlignedPointerInInternalField(0, nullptr);
    }

    /* Takes nothing, returns this */
    template <int SSL>
    void res_pause(args_t args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            res->pause();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns this */
    template <int SSL>
    void res_resume(args_t args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            res->resume();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, kills the connection */
    template <int SSL>
    void res_close(args_t args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            invalidateResObject(args);
            res->close();
            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes function of data and isLast. Expects nothing from callback, returns this */
    template <int SSL>
    void res_onData(args_t args) {
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

    /* Takes integer maxSize and function of fullData. Accumulates all data chunks and calls handler with the complete
     * body as an ArrayBuffer once all data has arrived. If the body exceeds maxSize bytes, handler is called with
     * null instead. Fast path: if all data arrives in a single chunk no allocation is made and the ArrayBuffer is
     * zero-copy (backed directly by the incoming data, detached after the call). Slow path: chunks are lazily
     * accumulated into a std::vector whose memory is transferred zero-copy into the ArrayBuffer backing store.
     * Returns this */
    template <int SSL>
    void res_collectBody(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            size_t maxSize = (size_t) args[0]->NumberValue(isolate->GetCurrentContext()).ToChecked();

            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[1]));

            /* Lazily allocated; nullptr means not yet started. Separate overflow flag distinguishes
             * the "not started" state from the "exceeded maxSize" state. */
            std::unique_ptr<std::vector<char>> buffer;
            bool overflow = false;

            res->onDataV2([p = std::move(p), buffer = std::move(buffer), overflow, maxSize, isolate](std::string_view data, uint64_t maxRemainingBodyLength) mutable {
                HandleScope hs(isolate);

                if (overflow) {
                    return;
                } else if (!buffer) {
                    /* First and possibly only chunk */
                    if (data.size() > maxSize) {
                        /* Overflow: return to JS with null */
                        overflow = true;
                        Local<Value> argv[] = {Null(isolate)};
                        CallJS(isolate, Local<Function>::New(isolate, p), 1, argv);
                    } else if (maxRemainingBodyLength == 0) {
                        /* Fast path: Single-chunk zero-copy: wrap data directly, detach after call like onData */
                        Local<ArrayBuffer> ab = ArrayBuffer_New(isolate, (void *) data.data(), data.size());
                        Local<Value> argv[] = {ab};
                        CallJS(isolate, Local<Function>::New(isolate, p), 1, argv);
                        ab->Detach();
                    } else {
                        /* Slow path begins: allocate buffer lazily for first non-terminal chunk */
                        buffer = std::make_unique<std::vector<char>>();
                        if (maxRemainingBodyLength <= maxSize - data.size()) {
                            /* Preallocate with hint */
                            buffer->reserve(maxRemainingBodyLength + data.size());
                        }
                        buffer->assign(data.begin(), data.end());
                    }
                } else if (data.size() > maxSize - buffer->size()) {
                    /* Subsequent chunks Overflow: return to JS with null */
                    buffer.reset();
                    overflow = true;
                    Local<Value> argv[] = {Null(isolate)};
                    CallJS(isolate, Local<Function>::New(isolate, p), 1, argv);
                } else {
                    /* Subsequent chunks: accumulate */
                    buffer->insert(buffer->end(), data.begin(), data.end());
                    if (maxRemainingBodyLength == 0) {
                        /* Zero-copy: hand V8 the vector's own memory via a custom deleter */
                        auto *rawBuffer = buffer.release();
                        auto backingStore = ArrayBuffer::NewBackingStore(
                            rawBuffer->data(), rawBuffer->size(),
                            [](void *, size_t, void *deleter_data) {
                                delete static_cast<std::vector<char> *>(deleter_data);
                            },
                            rawBuffer
                        );
                        Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(backingStore));
                        Local<Value> argv[] = {ab};
                        CallJS(isolate, Local<Function>::New(isolate, p), 1, argv);
                    }
                }
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes function of chunk and maxRemainingBodyLength. Returns this.
     * If maxRemainingBodyLength is 0, the last chunk has arrived. */
    template <int SSL>
    void res_onDataV2(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onDataV2([p = std::move(p), isolate](std::string_view data, uint64_t maxRemainingBodyLength) {
                HandleScope hs(isolate);

                Local<ArrayBuffer> dataArrayBuffer = ArrayBuffer_New(isolate, (void *) data.data(), data.length());

                /* Pass maxRemainingBodyLength so user can preallocate; 0 signals the last chunk */
                Local<Value> argv[] = {dataArrayBuffer, BigInt::NewFromUnsigned(isolate, maxRemainingBodyLength)};
                CallJS(isolate, Local<Function>::New(isolate, p), 2, argv);

                dataArrayBuffer->Detach();
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* Takes nothing, returns nothing. Cb wants nothing returned. */
    template <int SSL>
    void res_onAborted(args_t args) {
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
    void res_getRemoteAddress(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    void res_getRemoteAddressAsText(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns integer */
    template <int SSL>
    void res_getRemotePort(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            unsigned int port = res->getRemotePort();

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    void res_getProxiedRemoteAddress(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns arraybuffer */
    template <int SSL>
    void res_getProxiedRemoteAddressAsText(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            std::string_view ip = res->getProxiedRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns number */
    template <int SSL>
    void res_getProxiedRemotePort(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            unsigned int port = res->getProxiedRemotePort();

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    template <int PROTOCOL>
    void res_getX509Certificate(args_t args) {
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
    void res_getWriteOffset(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            args.GetReturnValue().Set(Number::New(isolate, getHttpResponse<SSL>(args)->getWriteOffset()));
        }
    }

    /* Takes function of bool(int), returns this */
    template <int SSL>
    void res_onWritable(args_t args) {
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
    void res_writeStatus(args_t args) {
        auto *res = getHttpResponse<SSL>(args);
            if (res) {
            NativeString<true> data(args.GetIsolate(), args[0]);
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
    void res_endWithoutBody(args_t args) {
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
    void res_end(args_t args) {
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString<true> data(args.GetIsolate(), args[0]);
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
    void res_tryEnd(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString<true> data(args.GetIsolate(), args[0]);
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
    void res_write(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            NativeString<true> data(args.GetIsolate(), args[0]);
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
    void res_writeHeader(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *res = getHttpResponse<PROTOCOL>(args);
        if (res) {
            // Optimization: writeHeader never calls JS or allocated on the GC
            // use zero copy string view in best case
            NativeString<true> header(args.GetIsolate(), args[0]);
            if (header.isInvalid(args)) {
                return;
            }
            NativeString<true> value(args.GetIsolate(), args[1]);
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
    void res_cork(args_t args) {
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
    void res_upgrade(args_t args) {
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
    template <int Option>
    Local<Object> init(Isolate *isolate) {
        Local<FunctionTemplate> resTemplate = FunctionTemplate::New(isolate);

        constexpr const char* classnames[4] = {
          "uWS.HttpResponse",
          "uWS.SSLHttpResponse", 
          "uWS.Http3Response",
          "uWS.CachedHttpResponse"
        };
        resTemplate->SetClassName(
            String::NewFromUtf8(isolate, classnames[static_cast<uint32_t>(Option)], NewStringType::kNormal).ToLocalChecked()
        );

        resTemplate->InstanceTemplate()->SetInternalFieldCount(1);

        Local<ObjectTemplate> resObjectTemplate = resTemplate->PrototypeTemplate();
        /* helper */
        auto regFn = [resObjectTemplate, isolate]<size_t N>(
          const char (&str)[N],
          void(*cb)(args_t)
        ){
          resObjectTemplate->Set(
            String::NewFromUtf8(isolate, str, NewStringType::kNormal, N-1).ToLocalChecked(),
            FunctionTemplate::New(isolate, cb)
          );
        };

        /* Register our functions */
        regFn("end", res_end<Option>);
        
        /* Cache has almost nothing wrapped yet */
        if constexpr (Option != 3) {
            regFn("writeStatus", res_writeStatus<Option>);
            regFn("endWithoutBody", res_endWithoutBody<Option>);
            regFn("tryEnd", res_tryEnd<Option>);
            regFn("write", res_write<Option>);
            regFn("writeHeader", res_writeHeader<Option>);
            regFn("close", res_close<Option>);
            regFn("onWritable", res_onWritable<Option>);
            regFn("onAborted", res_onAborted<Option>);
            regFn("onData", res_onData<Option>);
            
            /* QUIC has a lot of functions unimplemented */
            if constexpr (Option != 2) {
                regFn("onDataV2", res_onDataV2<Option>);
                regFn("collectBody", res_collectBody<Option>);
                regFn("getWriteOffset", res_getWriteOffset<Option>);
                regFn("getRemoteAddress", res_getRemoteAddress<Option>);
                regFn("cork", res_cork<Option>);

                // hmmmm...
                regFn("collect", res_cork<Option>);

                regFn("upgrade", res_upgrade<Option>);
                regFn("getRemoteAddressAsText", res_getRemoteAddressAsText<Option>);
                regFn("getRemotePort", res_getRemotePort<Option>);
                regFn("getProxiedRemoteAddress", res_getProxiedRemoteAddress<Option>);
                regFn("getProxiedRemoteAddressAsText", res_getProxiedRemoteAddressAsText<Option>);
                regFn("getProxiedRemotePort", res_getProxiedRemotePort<Option>);
                regFn("pause", res_pause<Option>);
                regFn("resume", res_resume<Option>);
            }
            if constexpr (Option == 1) {
              regFn("getX509Certificate", res_getX509Certificate<Option>);
            }
        }
        Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
                resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        
        /* Create our template */
        Local<Object> resObjectLocal = resTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return resObjectLocal;
    }};
