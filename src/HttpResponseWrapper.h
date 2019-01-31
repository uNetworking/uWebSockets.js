#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

struct HttpResponseWrapper {
    static Persistent<Object> resTemplate[2];

    template <bool SSL>
    static inline uWS::HttpResponse<SSL> *getHttpResponse(const FunctionCallbackInfo<Value> &args) {
        auto *res = (uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!res) {
            args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "Invalid access of discarded (invalid, deleted) uWS.HttpResponse/SSLHttpResponse.")));
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
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onData([p = std::move(p)](std::string_view data, bool last) {
                HandleScope hs(isolate);

                Local<ArrayBuffer> dataArrayBuffer = ArrayBuffer::New(isolate, (void *) data.data(), data.length());

                Local<Value> argv[] = {dataArrayBuffer, Boolean::New(isolate, last)};
                Local<Function>::New(isolate, p)->Call(isolate->GetCurrentContext()->Global(), 2, argv);

                dataArrayBuffer->Neuter();
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Takes nothing, returns nothing. Cb wants nothing returned. */
    template <bool SSL>
    static void res_onAborted(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            /* This is how we capture res (C++ this in invocation of this function) */
            UniquePersistent<Object> resObject(isolate, args.Holder());

            res->onAborted([p = std::move(p), resObject = std::move(resObject)]() {
                HandleScope hs(isolate);

                /* Mark this resObject invalid */
                Local<Object>::New(isolate, resObject)->SetAlignedPointerInInternalField(0, nullptr);

                Local<Function>::New(isolate, p)->Call(isolate->GetCurrentContext()->Global(), 0, nullptr);
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* Returns the current write offset */
    template <bool SSL>
    static void res_getWriteOffset(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            args.GetReturnValue().Set(Integer::New(isolate, getHttpResponse<SSL>(args)->getWriteOffset()));
        }
    }

    /* Takes function of bool(int), returns this */
    template <bool SSL>
    static void res_onWritable(const FunctionCallbackInfo<Value> &args) {
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            /* This thing perfectly fits in with unique_function, and will Reset on destructor */
            UniquePersistent<Function> p(isolate, Local<Function>::Cast(args[0]));

            res->onWritable([p = std::move(p)](int offset) {
                HandleScope hs(isolate);

                Local<Value> argv[] = {Integer::NewFromUnsigned(isolate, offset)};
                return Local<Function>::New(isolate, p)->Call(isolate->GetCurrentContext()->Global(), 1, argv)->BooleanValue();
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
        auto *res = getHttpResponse<SSL>(args);
        if (res) {
            NativeString data(args.GetIsolate(), args[0]);
            if (data.isInvalid(args)) {
                return;
            }

            int totalSize = 0;
            if (args.Length() > 1) {
                totalSize = args[1]->Uint32Value();
            }

            bool ok = res->tryEnd(data.getString(), totalSize);

            /* Invalidate this object if we responded completely */
            if (res->hasResponded()) {
                invalidateResObject(args);
            }

            /* This is a quick fix, it will need updating in µWS later on */
            Local<Array> array = Array::New(isolate, 2);
            array->Set(0, Boolean::New(isolate, ok));
            array->Set(1, Boolean::New(isolate, res->hasResponded()));

            args.GetReturnValue().Set(array);
        }
    }

    /* Takes data, returns true for success, false for backpressure */
    template <bool SSL>
    static void res_write(const FunctionCallbackInfo<Value> &args) {
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
            res->writeHeader(header.getString(),value.getString());

            args.GetReturnValue().Set(args.Holder());
        }
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
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close"), FunctionTemplate::New(isolate, res_close<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getWriteOffset"), FunctionTemplate::New(isolate, res_getWriteOffset<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onWritable"), FunctionTemplate::New(isolate, res_onWritable<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onAborted"), FunctionTemplate::New(isolate, res_onAborted<SSL>));
        resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "onData"), FunctionTemplate::New(isolate, res_onData<SSL>));

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
