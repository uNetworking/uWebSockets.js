/*
 * Authored by Alex Hultman, 2018-2021.
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

#pragma once

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <v8.h>
#include <memory>
#include <unordered_map>
#include "akeno/external/ankerl/unordered_dense.h"
using namespace v8;

namespace uWS {
    struct App;
}

namespace Akeno {
    class WebApp;
}

/* Unfortunately we _have_ to depend on Node.js crap */
#include <node.h>

MaybeLocal<Value> CallJS(Isolate *isolate, Local<Function> f, int argc, Local<Value> *argv) {
    extern int calledIntoJS;
    extern thread_local int insideCorkCallback;
    /* All calls we do into JS are properly corked, except for res.cork, where we increase the counter explicitly */
    insideCorkCallback++;
    /* Slow path */
    auto ret = node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), f, argc, argv, {0, 0});
    insideCorkCallback--;
    return ret;
}

Local<v8::ArrayBuffer> ArrayBuffer_New(Isolate *isolate, void *data, size_t length) {
    std::unique_ptr<BackingStore> backingStore = ArrayBuffer::NewBackingStore(data, length, [](void* data, size_t length, void* deleter_data) {}, nullptr);
    return ArrayBuffer::New(isolate, std::shared_ptr<BackingStore>(backingStore.release()));
}

Local<v8::ArrayBuffer> ArrayBuffer_NewCopy(Isolate *isolate, void *data, size_t length) {
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, length);
    memcpy(ab->GetBackingStore()->Data(), data, length);
    return ab;
}

struct PerSocketData {
    Global<Object> socketPf;
};

struct PerContextData {
    Isolate *isolate;
    Global<Object> reqTemplate[2]; // 0 = non-SSL/SSL, 1 = Http3
    Global<Object> resTemplate[4]; // 0 = non-SSL, 1 = SSL, 2 = Http3
    Global<Object> wsTemplate[2];

    /* We hold all apps and protocols until free */
    std::vector<std::unique_ptr<uWS::App>> apps;
    std::vector<std::unique_ptr<uWS::HTTPProtocol>> protocols;
    std::vector<std::unique_ptr<uWS::HTTPSProtocol>> sslProtocols;

    std::unordered_map<uWS::App *, std::shared_ptr<Global<Function>>> appObjectCallbacks;

    /* WebApp instances created from JS (kept alive for the isolate lifetime) */
    ankerl::unordered_dense::map<Akeno::WebApp *, std::shared_ptr<Akeno::WebApp>> webAppsByPtr;

    /* File processor callback and pending responses for async refresh */
    std::shared_ptr<Global<Function>> fileProcessorCallback;
    uint64_t nextFileProcessId = 1;

    struct PendingFileProcess {
        bool ssl = false;
        void *res = nullptr;
        Akeno::WebApp *webApp = nullptr;
        std::string url;
        std::string fullPath;
        std::string mimeType;
        std::string ifNoneMatch;
        std::string status;
        int variant = 0;
    };

    ankerl::unordered_dense::map<uint64_t, PendingFileProcess> pendingFileProcesses;
};

/* Returns the resTemplate / wsTemplate index for a protocol type.
 * 0 = non-SSL, 1 = SSL, 2 = H3 */
template <class PROTO>
static constexpr int getProtoTypeIndex() {
    if constexpr (std::is_same<PROTO, uWS::HTTPProtocol>::value) {
        return 0;
    } else if constexpr (std::is_same<PROTO, uWS::HTTPSProtocol>::value) {
        return 1;
    } else {
#ifndef UWS_NO_HTTP3
        if constexpr (std::is_same<PROTO, uWS::H3App>::value) {
            return 2;
        }
#endif
        return 0;
    }
}

static inline bool missingArguments(int length, const FunctionCallbackInfo<Value> &args) {
    if (args.Length() < length) {
        std::string message = "Function requires at least ";
        message += std::to_string(length);
        message += " arguments.";
        args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::Error(String::NewFromUtf8(args.GetIsolate(), message.c_str(), NewStringType::kNormal).ToLocalChecked())));
        return true;
    }
    return false;
}

struct Callback {
    bool invalid = false;
    UniquePersistent<Function> f;
    Callback(Isolate *isolate, const Local<Value> &value) {

        if (!value->IsFunction()) {
            invalid = true;
            return;
        }

        f.Reset(isolate, Local<Function>::Cast(value));
    }

    bool isInvalid(const FunctionCallbackInfo<Value> &args) {
        if (invalid) {
            args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::Error(String::NewFromUtf8(args.GetIsolate(), "Passed callback is not a valid function.", NewStringType::kNormal).ToLocalChecked())));
        }
        return invalid;
    }

    UniquePersistent<Function> &&getFunction() {
        return std::move(f);
    }
};

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
    bool invalid = false;
public:
    NativeString(Isolate *isolate, const Local<Value> &value) {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            utf8Value = new (utf8ValueMemory) String::Utf8Value(isolate, value);
            data = (**utf8Value);
            length = utf8Value->length();
        } else if (value->IsTypedArray()) {
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            auto contents = arrayBufferView->Buffer()->GetBackingStore();
            length = arrayBufferView->ByteLength();
            data = (char *) contents->Data() + arrayBufferView->ByteOffset();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            auto contents = arrayBuffer->GetBackingStore();
            length = contents->ByteLength();
            data = (char *) contents->Data();
        } else if (value->IsSharedArrayBuffer()) {
            Local<SharedArrayBuffer> arrayBuffer = Local<SharedArrayBuffer>::Cast(value);
            auto contents = arrayBuffer->GetBackingStore();
            length = contents->ByteLength();
            data = (char *) contents->Data();
        } else {
            invalid = true;
        }
    }

    bool isInvalid(const FunctionCallbackInfo<Value> &args) {
        if (invalid) {
            args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::Error(String::NewFromUtf8(args.GetIsolate(), "Text and data can only be passed by String, ArrayBuffer or TypedArray.", NewStringType::kNormal).ToLocalChecked())));
        }
        return invalid;
    }

    std::string_view getString() {
        return {data, length};
    }

    ~NativeString() {
        if (utf8Value) {
            utf8Value->~Utf8Value();
        }
    }
};

// Utility function to extract raw certificate data
std::string extractX509PemCertificate(SSL* ssl) {
    std::string pemCertificate;

    if (!ssl) {
        return pemCertificate;
    }

    // Get the peer certificate
    X509* peerCertificate = SSL_get_peer_certificate(ssl);
    if (!peerCertificate) {
        // No peer certificate available
        return pemCertificate;
    }

    // Convert X509 certificate to PEM format
    BIO* bio = BIO_new(BIO_s_mem());
    if(bio) {
        if (PEM_write_bio_X509(bio, peerCertificate)) {
            char* buffer;
            long length = BIO_get_mem_data(bio, &buffer);
            pemCertificate.assign(buffer, length);
        }
        BIO_free(bio);
    }

    // Free the peer certificate
    X509_free(peerCertificate);
    return pemCertificate;
}