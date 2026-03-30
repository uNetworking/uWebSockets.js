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
#include <vector>
#include <cstdlib>
using namespace v8;

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

class NativeStringContext {
    inline static thread_local std::vector<char> pool = std::vector<char>(8 * 1024 * 1024);
    size_t pool_offset = 0;
public:
    char *alloc(size_t size) {
        if (pool_offset + size > pool.size()) {
            return (char *) malloc(size);
        }
        char *ptr = pool.data() + pool_offset;
        pool_offset += size;
        return ptr;
    }

    void free(char *ptr) {
        if (ptr >= pool.data() && ptr < pool.data() + pool.size()) {
            return;
        }
        ::free(ptr);
    }
};

/**
 * Converts a UTF-16 string to UTF-8.
 * @param out Pointer to the destination buffer.
 * @param in  Pointer to the source UTF-16 buffer.
 * @param len Number of UTF-16 elements to process.
 */
static inline void utf16_to_utf8(char *out, const uint16_t *in, uint32_t len) {
    unsigned char *out_ptr = (unsigned char *) out;
    const uint16_t *in_end = in + len;
    while (in < in_end) {
        uint32_t cp = *in++;

        // Handle Surrogate Pairs
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (in < in_end && *in >= 0xDC00 && *in <= 0xDFFF) {
                cp = 0x10000 + ((cp & 0x3FF) << 10) + ((*in++) & 0x3FF);
            } else cp = 0xFFFD; // Unpaired high surrogate
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = 0xFFFD; // Unpaired low surrogate
        }

        // Encode Code Point into UTF-8 bytes
        if (cp <= 0x7F) {
            *out_ptr++ = (unsigned char) cp;
        } else if (cp <= 0x7FF) {
            *out_ptr++ = (unsigned char) ((cp >> 6) | 0xC0);
            *out_ptr++ = (unsigned char) ((cp & 0x3F) | 0x80);
        } else if (cp <= 0xFFFF) {
            *out_ptr++ = (unsigned char) ((cp >> 12) | 0xE0);
            *out_ptr++ = (unsigned char) (((cp >> 6) & 0x3F) | 0x80);
            *out_ptr++ = (unsigned char) ((cp & 0x3F) | 0x80);
        } else { // cp <= 0x10FFFF
            *out_ptr++ = (unsigned char) ((cp >> 18) | 0xF0);
            *out_ptr++ = (unsigned char) (((cp >> 12) & 0x3F) | 0x80);
            *out_ptr++ = (unsigned char) (((cp >> 6) & 0x3F) | 0x80);
            *out_ptr++ = (unsigned char) ((cp & 0x3F) | 0x80);
        }
    }
}

class NativeString {
    char *data;
    size_t length;
    bool invalid = false;
    bool allocated = false;
    NativeStringContext *ctx = nullptr;
public:
    NativeString(NativeStringContext &ctx, Isolate *isolate, const Local<Value> &value) : ctx(&ctx) {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            Local<String> string = Local<String>::Cast(value);
            #if NODE_MODULE_VERSION >= 137 // Node.js >= 24
                String::ValueView strView(isolate, string);
                if (strView.is_one_byte()) {
                    // utf8: direct access using ValueView
                    length = strView.length();
                    data = (char *) strView.data8();
                } else {
                    // utf16: convert and write to utf8
                    length = string->Utf8LengthV2(isolate);
                    data = ctx.alloc(length);
                    allocated = true;
                    utf16_to_utf8(data, strView.data16(), strView.length());
                }
            #else // Fallback Node.js < 24
                length = string->Utf8Length(isolate);
                data = ctx.alloc(length);
                allocated = true;
                string->WriteUtf8(isolate, data, length, nullptr, String::WriteOptions::NO_NULL_TERMINATION);
            #endif
        } else if (value->IsArrayBufferView()) { /* DataView or TypedArray */
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

    ~NativeString() noexcept {
        if (allocated) {
            ctx->free(data);
        }
    }

    NativeString(const NativeString &) = delete;
    NativeString &operator=(const NativeString &) = delete;

    bool isInvalid(const FunctionCallbackInfo<Value> &args) {
        if (invalid) {
            args.GetReturnValue().Set(args.GetIsolate()->ThrowException(v8::Exception::Error(String::NewFromUtf8(args.GetIsolate(), "Text and data can only be passed by String, ArrayBuffer, SharedArrayBuffer or ArrayBufferView.", NewStringType::kNormal).ToLocalChecked())));
        }
        return invalid;
    }

    std::string_view getString() {
        return {data, length};
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
