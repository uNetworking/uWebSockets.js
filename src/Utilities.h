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

#ifndef ADDON_UTILITIES_H
#define ADDON_UTILITIES_H

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <v8.h>
using namespace v8;

/* Getting internal pointer is different in recent V8 versions */
#if (V8_MAJOR_VERSION == 14)
    void *getInternalPointer(const Local<Object> &holder) {
        return holder->GetAlignedPointerFromInternalField(0, 0);
    }

    void setInternalPointer(const Local<Object> &holder, void *value) {
        holder->SetAlignedPointerInInternalField(0, value, 0);
    }
#else
    void *getInternalPointer(const Local<Object> &holder) {
        return holder->GetAlignedPointerFromInternalField(0);
    }

    void setInternalPointer(const Local<Object> &holder, void *value) {
        holder->SetAlignedPointerInInternalField(0, value);
    }
#endif

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
    UniquePersistent<Object> socketPf;
};

struct PerContextData {
    Isolate *isolate;
    UniquePersistent<Object> reqTemplate[2]; // 0 = non-SSL/SSL, 1 = Http3
    UniquePersistent<Object> resTemplate[4]; // 0 = non-SSL, 1 = SSL, 2 = Http3
    UniquePersistent<Object> wsTemplate[2];

    /* We hold all apps until free */
    std::vector<std::unique_ptr<uWS::App>> apps;
    std::vector<std::unique_ptr<uWS::SSLApp>> sslApps;
};

template <class APP>
static constexpr int getAppTypeIndex() {
    /* Returns 1 for SSLApp and 0 for App */
    //return std::is_same<APP, uWS::SSLApp>::value;

    /* Returns 2 for H3App */

    if constexpr (std::is_same<APP, uWS::App>::value) {
        return 0;
    } else if constexpr (std::is_same<APP, uWS::SSLApp>::value) {
        return 1;
    } else if constexpr (std::is_same<APP, uWS::H3App>::value) {
        return 2;
    } else {
        // why does this fail?
        //static_assert(false);
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

template <bool AllowStringView = false>
class NativeString {
    char *data;
    size_t length;
    bool needsFree = false;
    bool invalid = false;

    // Static thread-local state shared by all NativeString instances on this thread
    // Pool size must be a multiple of 8 because pool_offset is 8-byte aligned
    inline static thread_local std::vector<char> pool = std::vector<char>(128 * 1024);
    inline static thread_local size_t pool_offset = 0;
    inline static thread_local int ref_count = 0;

    static char* getPoolPtr(size_t size) {
        if (size > pool.size() - pool_offset) {
            return nullptr;
        }
        return pool.data() + pool_offset;
    }

    static void commitPool(size_t size) {
        // pool_offset is 8-byte aligned for access performance
        pool_offset += (size + 7) & ~size_t{7};
    }

    static void clearPool() {
        pool_offset = 0;
    }

public:
    NativeString(const NativeString&) = delete;
    NativeString& operator=(const NativeString&) = delete;
    NativeString(NativeString&&) = delete;
    NativeString& operator=(NativeString&&) = delete;

    NativeString(Isolate *isolate, const Local<Value> &value) {
        if (ref_count == 0) {
            clearPool();
        }
        ref_count++;

        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            Local<String> string = Local<String>::Cast(value);
            const size_t stringLength = static_cast<size_t>(string->Length());
            size_t allocSize;

            // Get the worst-case allocation size
            if (string->IsOneByte()) { // Latin-1 -> UTF-8
                allocSize = 2 * stringLength;
            } else { // UTF-16 -> UTF-8
                allocSize = 3 * stringLength;
            }

            // Allocate data
            data = getPoolPtr(allocSize);
            if (data == nullptr) {
                // Worst-case size can't be pooled: calculate the exact size
                #if (V8_MAJOR_VERSION == 14)
                    allocSize = string->Utf8LengthV2(isolate);
                #else
                    allocSize = string->Utf8Length(isolate);
                #endif

                data = getPoolPtr(allocSize);
                if (data == nullptr) {
                    // Still can't be pooled: heap allocate it
                    data = static_cast<char*>(std::malloc(allocSize));
                    if (data == nullptr) {
                        invalid = true;
                        return;
                    }
                    needsFree = true;
                }
            }

            // Convert to UTF-8 into data and get the number of bytes written
            #if (V8_MAJOR_VERSION == 14)
                length = string->WriteUtf8V2(isolate, data, allocSize);
            #else
                length = string->WriteUtf8(isolate, data, allocSize, nullptr, String::WriteOptions::NO_NULL_TERMINATION);
            #endif
            if (!needsFree) {
                commitPool(length);
            }
        } else if (value->IsArrayBufferView()) { /* DataView or TypedArray */
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            Local<ArrayBuffer> arrayBuffer = arrayBufferView->Buffer();
            length = arrayBufferView->ByteLength();
            data = (char *) arrayBuffer->Data() + arrayBufferView->ByteOffset();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            length = arrayBuffer->ByteLength();
            data = (char *) arrayBuffer->Data();
        } else if (value->IsSharedArrayBuffer()) {
            Local<SharedArrayBuffer> arrayBuffer = Local<SharedArrayBuffer>::Cast(value);
            length = arrayBuffer->ByteLength();
            data = (char *) arrayBuffer->Data();
        } else {
            invalid = true;
        }
    }

    bool isInvalid(const FunctionCallbackInfo<Value> &args) {
        if (invalid) {
            args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::Error(String::NewFromUtf8(args.GetIsolate(), "Text and data can only be passed by String, ArrayBuffer or ArrayBufferView.", NewStringType::kNormal).ToLocalChecked())));
        }
        return invalid;
    }

    std::string_view getString() {
        return {data, length};
    }

    ~NativeString() {
        ref_count--;
        if (needsFree) {
            ::free(data);
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

#endif
