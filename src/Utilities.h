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

/* v8::Global's move constructor is not noexcept, so a lambda capturing one fails
 * MoveOnlyFunction's small-object test and heap-allocates. The underlying move is
 * a pointer swap and cannot throw, this wrapper only restores the noexcept */
template <class T>
struct NoexceptPersistent {
    Global<T> p;

    NoexceptPersistent() : p() {}
    NoexceptPersistent(Isolate *isolate, const Local<T> &v) : p(isolate, v) {}
    NoexceptPersistent(NoexceptPersistent &&other) noexcept : p(std::move(other.p)) {}

    NoexceptPersistent& operator=(NoexceptPersistent &&other) noexcept {
        if (this != &other) {
            p = std::move(other.p);
        }
        return *this;
    }

    NoexceptPersistent(const NoexceptPersistent &) = delete;
    NoexceptPersistent &operator=(const NoexceptPersistent &) = delete;

    bool IsEmpty() const { return p.IsEmpty(); }
    void Reset() { p.Reset(); }
    void Reset(Isolate *isolate, const Local<T> &v) { p.Reset(isolate, v); }
    Local<T> Get(Isolate *isolate) const { return p.Get(isolate); }

    operator const Global<T>&() const { return p; }
    operator Global<T>&() { return p; }

    template <typename S>
    bool operator==(const Local<S> &other) const { return p == other; }

    template <typename S>
    bool operator!=(const Local<S> &other) const { return p != other; }
};
static_assert(std::is_nothrow_move_constructible<NoexceptPersistent<Function>>::value, "NoexceptPersistent must be nothrow movable");

struct PerSocketData {
    NoexceptPersistent<Object> socketPf;
};

struct PerContextData {
    Isolate *isolate;
    NoexceptPersistent<Object> reqTemplate[2]; // 0 = non-SSL/SSL, 1 = Http3
    NoexceptPersistent<Object> resTemplate[4]; // 0 = non-SSL, 1 = SSL, 2 = Http3
    NoexceptPersistent<Object> wsTemplate[2];

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
    NoexceptPersistent<Function> f;
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

    NoexceptPersistent<Function> &&getFunction() {
        return std::move(f);
    }
};

template <bool AllowStringView = false>
class NativeString {
    char *data;
    size_t length;
    bool allocated = false;
    bool invalid = false;

    // State shared by all NativeString instances on this thread, in one thread-local read once per
    // instance: in an addon every thread-local access is a call into the dynamic linker
    struct Pool {
        std::vector<char> buffer = std::vector<char>(128 * 1024);
        size_t offset = 0;
        int refCount = 0;
    };
    inline static thread_local Pool threadPool;
    Pool *pool;

    char* alloc(size_t size) {
        // Ensure size is a multiple of 8
        size = (size + 7) & ~7;

        // Fallback for allocations larger than the remaining pool space, or that reach its end:
        // on a full pool 0 bytes would get the end pointer, and free() would pass it to ::free
        if (pool->offset + size >= pool->buffer.size()) {
            // Mark for external cleanup if using instance-based logic
            // (Note: In a pure static alloc, you'd need a way to track this)
            return (char*)std::malloc(size);
        }

        char* ptr = pool->buffer.data() + pool->offset;
        pool->offset += size;
        return ptr;
    }

    // Provided for completeness, though the "pool" doesn't actually free individual slices
    void free(char* ptr) {
        if (ptr < pool->buffer.data() || ptr >= pool->buffer.data() + pool->buffer.size()) {
            ::free(ptr);
        }
    }

public:
    NativeString(Isolate *isolate, const Local<Value> &value) {
        pool = &threadPool;
        if (pool->refCount == 0) {
            pool->offset = 0; // Reset the "stack" when entering the first scope
        }
        pool->refCount++;

        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            Local<String> string = Local<String>::Cast(value);

            /* One pass: write straight into what is left of the pool and keep the bytes written.
             * When not all characters fit, measure the string and write it again.
             * With more characters than free bytes it can never fit: skip the first write.
             * A two-byte string is slow to write and takes up to 3 bytes per unit: past a third of
             * the free pool it is measured first, so a write that does not fit is never thrown away */
            size_t capacity = pool->buffer.size() - pool->offset;
            bool fits = false;
            /* Should really be string->IsOneByte() ? 2 : 3 but we kept Latin-1 as 1 byte UTF-8 for deliberate reasons */
            if ((size_t) string->Length() * (string->IsOneByte() ? 1 : 3) <= capacity) {
            #if (V8_MAJOR_VERSION == 14)
                size_t processed = 0;
                length = string->WriteUtf8V2(isolate, pool->buffer.data() + pool->offset, capacity, String::WriteFlags::kNone, &processed);
                fits = capacity && processed == (size_t) string->Length();
            #else
                int processed = 0;
                length = string->WriteUtf8(isolate, pool->buffer.data() + pool->offset, (int) capacity, &processed, String::WriteOptions::NO_NULL_TERMINATION);
                fits = capacity && processed == string->Length();
            #endif
            }

            if (fits) {
                data = pool->buffer.data() + pool->offset;
                pool->offset += (length + 7) & ~7;
            } else {
                #if (V8_MAJOR_VERSION == 14)
                    length = string->Utf8LengthV2(isolate);
                    data = alloc(length);
                    string->WriteUtf8V2(isolate, data, length);
                #else
                    length = string->Utf8Length(isolate);
                    data = alloc(length);
                    string->WriteUtf8(isolate, data, length, nullptr, String::WriteOptions::NO_NULL_TERMINATION);
                #endif
            }
            allocated = true;


        } else if (value->IsArrayBufferView()) { /* DataView or TypedArray */
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            length = arrayBufferView->ByteLength();
            data = (char *) arrayBufferView->Buffer()->Data() + arrayBufferView->ByteOffset();
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
        pool->refCount--;
        if (allocated) {
            free(data);
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
