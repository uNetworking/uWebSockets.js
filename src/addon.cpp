/*
 * Authored by Alex Hultman, 2018-2019.
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

/* We are only allowed to depend on µWS and V8 in this layer. */
#include "App.h"

#include <iostream>
#include <vector>
#include <type_traits>

/* This one can never change for the duration of this process, so never mind per context data:ing it, yes that is a word now */
bool experimental_fastcall = 0;

#include <v8.h>
using namespace v8;

/* Compatibility for V8 7.0 and earlier */
#include <v8-version.h>
bool BooleanValue(Isolate *isolate, Local<Value> value) {
    #if V8_MAJOR_VERSION < 7 || (V8_MAJOR_VERSION == 7 && V8_MINOR_VERSION == 0)
        /* Old */
        return value->BooleanValue(isolate->GetCurrentContext()).ToChecked();
    #else
        /* Node.js 12, 13 */
        return value->BooleanValue(isolate);
    #endif
}

void NeuterArrayBuffer(Local<ArrayBuffer> ab) {
    #if V8_MAJOR_VERSION < 7 || (V8_MAJOR_VERSION == 7 && V8_MINOR_VERSION == 0)
        /* Old */
        ab->Neuter();
    #else
        /* Node.js 12, 13 */
        ab->Detach();
    #endif
}

#include "Utilities.h"
#include "WebSocketWrapper.h"
#include "HttpResponseWrapper.h"
#include "HttpRequestWrapper.h"
#include "AppWrapper.h"

#include <numeric>
#include <functional>

/* Todo: Apps should be freed once the GC says so BUT ALWAYS before freeing the loop */

/* Pass various undocumented configs */
void uWS_cfg(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    int keyCode = std::accumulate(key.getString().begin(), key.getString().end(), 1, std::plus<int>());
    if (keyCode == 656) {
        uWS::Loop::get()->setSilent(true);
    }
}

/* This has to be called in beforeExit, but exit also seems okay */
void uWS_free(const FunctionCallbackInfo<Value> &args) {
    /* Holder is exports */
    Local<Object> exports = args.Holder();

    /* See if we even have free anymore */
    if (exports->Get(args.GetIsolate()->GetCurrentContext(), String::NewFromUtf8(args.GetIsolate(), "free", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked() == Undefined(args.GetIsolate())) {
        return;
    }

    /* Once we free uWS we remove the uWS.free function from exports */
    exports->Set(args.GetIsolate()->GetCurrentContext(), String::NewFromUtf8(args.GetIsolate(), "free", NewStringType::kNormal).ToLocalChecked(), Undefined(args.GetIsolate())).ToChecked();

    /* We get the External holding perContextData */
    PerContextData *perContextData = (PerContextData *) Local<External>::Cast(args.Data())->Value();

    /* Freeing apps here, it could be done earlier but not sooner */
    perContextData->apps.clear();
    perContextData->sslApps.clear();
    /* Freeing the loop here means we give time for our timers to close, etc */
    uWS::Loop::get()->free();

    /* We can safely delete this since we no longer can call uWS.free */
    delete perContextData;
}

/* todo: Put this function and all inits of it in its own header */
void uWS_us_listen_socket_close(const FunctionCallbackInfo<Value> &args) {
    // this should take int ssl first
    us_listen_socket_close(0, (struct us_listen_socket_t *) External::Cast(*args[0])->Value());
}

/* Temporary KV store (doesn't belong here) */
#include <unordered_map>
#include <string>
#include <mutex>

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> kvStoreString;
std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> kvStoreInteger;
std::mutex kvMutex;

// getString(key, collection)
void uWS_getString(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    NativeString collection(args.GetIsolate(), args[1]);
    if (collection.isInvalid(args)) {
        return;
    }

    std::string value = kvStoreString[std::string(collection.getString())][std::string(key.getString())];

    args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), value.data(), NewStringType::kNormal, value.length()).ToLocalChecked());
}

void uWS_setString(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }
    NativeString value(args.GetIsolate(), args[1]);
    if (value.isInvalid(args)) {
        return;
    }

    NativeString collection(args.GetIsolate(), args[2]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreString[std::string(collection.getString())][std::string(key.getString())] = value.getString();
}

void uWS_getInteger(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    NativeString collection(args.GetIsolate(), args[1]);
    if (collection.isInvalid(args)) {
        return;
    }

    uint32_t value = kvStoreInteger[std::string(collection.getString())][std::string(key.getString())];

    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), value));
}

void uWS_setInteger(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    uint32_t value = Local<Integer>::Cast(args[1])->Value();

    NativeString collection(args.GetIsolate(), args[2]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreInteger[std::string(collection.getString())][std::string(key.getString())] = value;
}

void uWS_incInteger(const FunctionCallbackInfo<Value> &args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    uint32_t change = Local<Integer>::Cast(args[1])->Value();

    NativeString collection(args.GetIsolate(), args[2]);
    if (collection.isInvalid(args)) {
        return;
    }

    uint32_t value = kvStoreInteger[std::string(collection.getString())][std::string(key.getString())] += change;

    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), value));
}

/* This one will spike memory usage for large stores */
void uWS_getStringKeys(const FunctionCallbackInfo<Value> &args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    Local<Array> stringKeys = Array::New(args.GetIsolate(), kvStoreString.size());

    int offset = 0;

    for (auto p : kvStoreString[std::string(collection.getString())]) {
        stringKeys->Set(args.GetIsolate()->GetCurrentContext(), offset++, String::NewFromUtf8(args.GetIsolate(), p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked());
    }

    args.GetReturnValue().Set(stringKeys);
}

void uWS_getIntegerKeys(const FunctionCallbackInfo<Value> &args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    Local<Array> integerKeys = Array::New(args.GetIsolate(), kvStoreInteger.size());

    int offset = 0;

    for (auto p : kvStoreInteger[std::string(collection.getString())]) {
        integerKeys->Set(args.GetIsolate()->GetCurrentContext(), offset++, String::NewFromUtf8(args.GetIsolate(), p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked());
    }

    args.GetReturnValue().Set(integerKeys);
}

void uWS_deleteString(const FunctionCallbackInfo<Value> &args) {

    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    NativeString collection(args.GetIsolate(), args[1]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreString[std::string(collection.getString())].erase(std::string(key.getString()));

    //args.GetReturnValue().Set(Integer::New(args.GetIsolate(), value));
}

void uWS_deleteInteger(const FunctionCallbackInfo<Value> &args) {

    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    NativeString collection(args.GetIsolate(), args[1]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreInteger[std::string(collection.getString())].erase(std::string(key.getString()));

    //args.GetReturnValue().Set(Integer::New(args.GetIsolate(), value));
}

void uWS_deleteStringCollection(const FunctionCallbackInfo<Value> &args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreString.erase(std::string(collection.getString()));

    //args.GetReturnValue().Set(integerKeys);
}

void uWS_deleteIntegerCollection(const FunctionCallbackInfo<Value> &args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreInteger.erase(std::string(collection.getString()));

    //args.GetReturnValue().Set(integerKeys);
}

void uWS_lock(const FunctionCallbackInfo<Value> &args) {
    kvMutex.lock();
}

void uWS_unlock(const FunctionCallbackInfo<Value> &args) {
    kvMutex.unlock();
}

void Main(Local<Object> exports) {

    /* We only care if it is defined, not what it says */
    experimental_fastcall = getenv("EXPERIMENTAL_FASTCALL") != nullptr;

    /* We pass isolate everywhere */
    Isolate *isolate = exports->GetIsolate();

    if (experimental_fastcall) {
        /* We want this so that we can redefine process.nextTick to using the V8 native microtask queue */
        /* Settings this crashes Node.js while debugging with breakpoints */
        isolate->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);
    }

    /* Init the template objects, SSL and non-SSL, store it in per context data */
    PerContextData *perContextData = new PerContextData;
    perContextData->isolate = isolate;
    perContextData->reqTemplate.Reset(isolate, HttpRequestWrapper::init(isolate));
    perContextData->resTemplate[0].Reset(isolate, HttpResponseWrapper::init<0>(isolate));
    perContextData->resTemplate[1].Reset(isolate, HttpResponseWrapper::init<1>(isolate));
    perContextData->wsTemplate[0].Reset(isolate, WebSocketWrapper::init<0>(isolate));
    perContextData->wsTemplate[1].Reset(isolate, WebSocketWrapper::init<1>(isolate));

    /* Refer to per context data via External */
    Local<External> externalPerContextData = External::New(isolate, perContextData);

    /* uWS namespace */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "App", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App<uWS::App>, externalPerContextData)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "SSLApp", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App<uWS::SSLApp>, externalPerContextData)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "free", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_free, externalPerContextData)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Temporary KV store */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "getString", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_getString)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "setString", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_setString)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "getInteger", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_getInteger)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "setInteger", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_setInteger)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "incInteger", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_incInteger)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "lock", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_lock)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "unlock", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_unlock)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "getIntegerKeys", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_getIntegerKeys)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "getStringKeys", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_getStringKeys)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "deleteString", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_deleteString)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "deleteInteger", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_deleteInteger)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "deleteStringCollection", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_deleteStringCollection)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "deleteIntegerCollection", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_deleteIntegerCollection)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "_cfg", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_cfg)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Expose some µSockets functions directly under uWS namespace */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "us_listen_socket_close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_us_listen_socket_close)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Compression enum */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DISABLED", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DISABLED)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "SHARED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::SHARED_COMPRESSOR)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_3KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_3KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_4KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_4KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_8KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_8KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_16KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_16KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_32KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_32KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_64KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_64KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_128KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_128KB)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR_256KB", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR_256KB)).ToChecked();

    /* Listen options */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "LIBUS_LISTEN_EXCLUSIVE_PORT", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, LIBUS_LISTEN_EXCLUSIVE_PORT)).ToChecked();
}

/* This is required when building as a Node.js addon */
#ifndef ADDON_IS_HOST
#include <node.h>
extern "C" NODE_MODULE_EXPORT void
NODE_MODULE_INITIALIZER(Local<Object> exports, Local<Value> module, Local<Context> context) {
    /* Integrate uSockets with existing libuv loop */
    uWS::Loop::get(node::GetCurrentEventLoop(context->GetIsolate()));
    /* Register vanilla V8 addon */
    Main(exports);
}
#endif
