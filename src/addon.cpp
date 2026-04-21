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

/* We are only allowed to depend on µWS and V8 in this layer. */


#include <iostream>
#include <vector>
#include <type_traits>

#include "Utilities.h"
#include "WebSocketWrapper.h"
#include "HttpResponseWrapper.h"
#include "HttpRequestWrapper.h"
#include "AppWrapper.h"

#include <numeric>
#include <functional>

/* Todo: Apps should be freed once the GC says so BUT ALWAYS before freeing the loop */

#include "Multipart.h"

/* This function is somewhat of a simplifying wrapper that does not follow the C++ library.
 * It takes a POST:ed body and contentType, and returns an array of parts if
 * the request is a multipart request */
void uWS_getParts(args_t args) {

    /* Because we mutate the strings, it is important that we get mutable input like
     * ArrayBuffer or Buffer, not String! */
    Isolate *isolate = args.GetIsolate();

    NativeString body(args.GetIsolate(), args[0]);
    if (body.isInvalid(args)) {
        return;
    }

    NativeString contentType(args.GetIsolate(), args[1]);
    if (contentType.isInvalid(args)) {
        return;
    }

    uWS::MultipartParser mp(contentType.getString());
    if (mp.isValid()) {
        mp.setBody(body.getString());

        std::pair<std::string_view, std::string_view> headers[10];

        Local<Array> parts = Array::New(args.GetIsolate(), 0);

        while (true) {
            std::optional<std::string_view> optionalPart = mp.getNextPart(headers);
            if (!optionalPart.has_value()) {
                break;
            }

            std::string_view part = optionalPart.value();

            Local<ArrayBuffer> partArrayBuffer = ArrayBuffer_NewCopy(isolate, (void *) part.data(), part.length());
            /* Map is 30% faster in this case, but a static Object could be faster still */
            Local<Object> partMap = Object::New(isolate);
            partMap->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "data", NewStringType::kNormal).ToLocalChecked(), partArrayBuffer).IsNothing();

            for (int i = 0; headers[i].first.length(); i++) {
                /* We care about content-type and content-disposition */
                if (headers[i].first == "content-type") {
                    partMap->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "type", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, headers[i].second.data(), NewStringType::kNormal, headers[i].second.length()).ToLocalChecked()).IsNothing();
                } else if (headers[i].first == "content-disposition") {
                    /* Parse the parameters */
                    uWS::ParameterParser pp(headers[i].second);
                    while (true) {
                        auto [key, value] = pp.getKeyValue();
                        if (!key.length()) {
                            break;
                        }

                        // really anything that has both key and value and is not type or data?
                        if (key == "name" || key == "filename") {
                            partMap->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, key.data(), NewStringType::kNormal, key.length()).ToLocalChecked(), String::NewFromUtf8(isolate, value.data(), NewStringType::kNormal, value.length()).ToLocalChecked()).IsNothing();
                        }
                    }
                }
            }

            parts->Set(isolate->GetCurrentContext(), parts->Length(), partMap).IsNothing();
        }

        args.GetReturnValue().Set(parts);
    }

    /* We'll return undefined on error */
}

/* Faster setTimeout, clearTimeout */

//#include "FastTimers.h"

//UniquePersistent<Function> timerCallbacksJS[1000];

void uWS_arm(args_t args) {

    /* integer */

    uint32_t ms = Local<Integer>::Cast(args[0])->Value();


    //unsigned int timer = setTimeout_(nullptr, 1000);
}

void uWS_setTimeout(args_t args) {

    /* Function, integer */

    //unsigned int timer = setTimeout_(nullptr, 1000);

    //timerCallbacksJS[timer].Reset(args.GetIsolate(), Local<Function>::Cast(args[0]));

    //args.GetReturnValue().Set(Integer::New(args.GetIsolate(), timer));
}

void uWS_clearTimeout(args_t args) {

    /* Integer */

    uint32_t timer = Local<Integer>::Cast(args[0])->Value();

    //clearTimeout_(timer);

    //timerCallbacksJS[timer].Reset();
}

/* Pass various undocumented configs */
void uWS_cfg(args_t args) {
    NativeString key(args.GetIsolate(), args[0]);
    if (key.isInvalid(args)) {
        return;
    }

    int keyCode = std::accumulate(key.getString().begin(), key.getString().end(), 1, std::plus<int>());
    if (keyCode == 656) {
        uWS::Loop::get()->setSilent(true);
    }
}

/* todo: Put this function and all inits of it in its own header */
void uWS_us_listen_socket_close(args_t args) {
    // this should take int ssl first
    us_listen_socket_close(0, (struct us_listen_socket_t *) External::Cast(*args[0])->Value());
}

void uWS_us_socket_local_port(args_t args) {
    // this should take int ssl first, but us_socket_local_port doesn't use it so it doesn't matter
    int port = us_socket_local_port(0, (struct us_socket_t *) External::Cast(*args[0])->Value());
    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), port));
}

/* Temporary KV store (doesn't belong here) */
#include <unordered_map>
#include <string>
#include <mutex>

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> kvStoreString;
std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> kvStoreInteger;
std::mutex kvMutex;

// getString(key, collection)
void uWS_getString(args_t args) {
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

void uWS_setString(args_t args) {
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

void uWS_getInteger(args_t args) {
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

void uWS_setInteger(args_t args) {
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

void uWS_incInteger(args_t args) {
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
void uWS_getStringKeys(args_t args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    Local<Array> stringKeys = Array::New(args.GetIsolate(), kvStoreString.size());

    int offset = 0;

    for (auto p : kvStoreString[std::string(collection.getString())]) {
        stringKeys->Set(args.GetIsolate()->GetCurrentContext(), offset++, String::NewFromUtf8(args.GetIsolate(), p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked()).IsNothing();
    }

    args.GetReturnValue().Set(stringKeys);
}

void uWS_getIntegerKeys(args_t args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    Local<Array> integerKeys = Array::New(args.GetIsolate(), kvStoreInteger.size());

    int offset = 0;

    for (auto p : kvStoreInteger[std::string(collection.getString())]) {
        integerKeys->Set(args.GetIsolate()->GetCurrentContext(), offset++, String::NewFromUtf8(args.GetIsolate(), p.first.data(), NewStringType::kNormal, p.first.length()).ToLocalChecked()).IsNothing();
    }

    args.GetReturnValue().Set(integerKeys);
}

void uWS_deleteString(args_t args) {

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

void uWS_deleteInteger(args_t args) {

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

void uWS_deleteStringCollection(args_t args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreString.erase(std::string(collection.getString()));

    //args.GetReturnValue().Set(integerKeys);
}

void uWS_deleteIntegerCollection(args_t args) {

    NativeString collection(args.GetIsolate(), args[0]);
    if (collection.isInvalid(args)) {
        return;
    }

    kvStoreInteger.erase(std::string(collection.getString()));

    //args.GetReturnValue().Set(integerKeys);
}

void uWS_lock(args_t args) {
    kvMutex.lock();
}

void uWS_unlock(args_t args) {
    kvMutex.unlock();
}

PerIsolateData *Main(Local<Object> exports) {

    /* We pass isolate everywhere */
    Isolate *isolate = exports->GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    /* Init the template objects, SSL and non-SSL, store it in per context data */
    PerIsolateData *perIsolateData = new PerIsolateData;
    perIsolateData->isolate = isolate;
    perIsolateData->reqTemplate[0].Reset(isolate, HttpRequestWrapper::init<0>(isolate));
    perIsolateData->reqTemplate[1].Reset(isolate, HttpRequestWrapper::init<2>(isolate));
    perIsolateData->resTemplate[0].Reset(isolate, HttpResponseWrapper::init<0>(isolate));
    perIsolateData->resTemplate[1].Reset(isolate, HttpResponseWrapper::init<1>(isolate));
    perIsolateData->resTemplate[2].Reset(isolate, HttpResponseWrapper::init<2>(isolate));
    perIsolateData->resTemplate[3].Reset(isolate, HttpResponseWrapper::init<3>(isolate));
    perIsolateData->wsTemplate[0].Reset(isolate, WebSocketWrapper::init<0>(isolate));
    perIsolateData->wsTemplate[1].Reset(isolate, WebSocketWrapper::init<1>(isolate));

    /* Refer to per context data via External */
    Local<External> externalPerContextData = External::New(isolate, perIsolateData);

    /* Helpers to write almost NOTHING manually */
    auto regFn = [=]<size_t N>(
      const char (&str)[N],
      void(*cb)(args_t)
    ){
      exports->Set(
        context,
        String::NewFromUtf8(isolate, str, NewStringType::kNormal, N-1).ToLocalChecked(),
        FunctionTemplate::New(isolate, cb, externalPerContextData)->GetFunction(context).ToLocalChecked()
      ).ToChecked();
    };
    auto regNum = [=]<size_t N>(
      const char (&str)[N],
      uint32_t number
    ){
      exports->Set(
        context,
        String::NewFromUtf8(isolate, str, NewStringType::kNormal, N-1).ToLocalChecked(),
        Integer::NewFromUnsigned(isolate, number)
      ).ToChecked();
    };

    /* uWS namespace */
    regFn("App", uWS_App<uWS::App>);
    regFn("SSLApp", uWS_App<uWS::SSLApp>);
    
    /* H3 experimental */
    regFn("H3App", uWS_App<uWS::H3App>);

    /* Temporary KV store */
    
    regFn("getString",  uWS_getString);
    regFn("setString",  uWS_setString);
    regFn("getInteger",  uWS_getInteger);
    regFn("setInteger",  uWS_setInteger);
    regFn("incInteger",  uWS_incInteger);
    regFn("lock",  uWS_lock);
    regFn("unlock",  uWS_unlock);
    regFn("getIntegerKeys",  uWS_getIntegerKeys);
    regFn("getStringKeys",  uWS_getStringKeys);
    regFn("deleteString",  uWS_deleteString);
    regFn("deleteInteger",  uWS_deleteInteger);
    regFn("deleteStringCollection",  uWS_deleteStringCollection);
    regFn("deleteIntegerCollection",  uWS_deleteIntegerCollection);

    regFn("setTimeout",  uWS_setTimeout);
    regFn("clearTimeout",  uWS_clearTimeout);
    regFn("arm",  uWS_arm);

    regFn("_cfg",  uWS_cfg);
    regFn("getParts",  uWS_getParts);
    
    /* Expose some µSockets functions directly under uWS namespace */
    regFn("us_listen_socket_close",  uWS_us_listen_socket_close);
    regFn("us_socket_local_port",  uWS_us_socket_local_port);

    /* Compression enum */
    regNum("DISABLED", uWS::DISABLED);
    regNum("SHARED_COMPRESSOR", uWS::SHARED_COMPRESSOR);
    regNum("SHARED_DECOMPRESSOR", uWS::SHARED_DECOMPRESSOR);
    regNum("DEDICATED_DECOMPRESSOR", uWS::DEDICATED_DECOMPRESSOR);
    regNum("DEDICATED_COMPRESSOR", uWS::DEDICATED_COMPRESSOR);
    regNum("DEDICATED_COMPRESSOR_3KB", uWS::DEDICATED_COMPRESSOR_3KB);
    regNum("DEDICATED_COMPRESSOR_4KB", uWS::DEDICATED_COMPRESSOR_4KB);
    regNum("DEDICATED_COMPRESSOR_8KB", uWS::DEDICATED_COMPRESSOR_8KB);
    regNum("DEDICATED_COMPRESSOR_16KB", uWS::DEDICATED_COMPRESSOR_16KB);
    regNum("DEDICATED_COMPRESSOR_32KB", uWS::DEDICATED_COMPRESSOR_32KB);
    regNum("DEDICATED_COMPRESSOR_64KB", uWS::DEDICATED_COMPRESSOR_64KB);
    regNum("DEDICATED_COMPRESSOR_128KB", uWS::DEDICATED_COMPRESSOR_128KB);
    regNum("DEDICATED_COMPRESSOR_256KB", uWS::DEDICATED_COMPRESSOR_256KB);

    regNum("DEDICATED_DECOMPRESSOR_32KB", uWS::DEDICATED_DECOMPRESSOR_32KB);
    regNum("DEDICATED_DECOMPRESSOR_16KB", uWS::DEDICATED_DECOMPRESSOR_16KB);
    regNum("DEDICATED_DECOMPRESSOR_8KB", uWS::DEDICATED_DECOMPRESSOR_8KB);
    regNum("DEDICATED_DECOMPRESSOR_4KB", uWS::DEDICATED_DECOMPRESSOR_4KB);
    regNum("DEDICATED_DECOMPRESSOR_2KB", uWS::DEDICATED_DECOMPRESSOR_2KB);
    regNum("DEDICATED_DECOMPRESSOR_1KB", uWS::DEDICATED_DECOMPRESSOR_1KB);
    regNum("DEDICATED_DECOMPRESSOR_512B", uWS::DEDICATED_DECOMPRESSOR_512B);

    /* Listen options */
    regNum("LIBUS_LISTEN_EXCLUSIVE_PORT", LIBUS_LISTEN_EXCLUSIVE_PORT);

    return perIsolateData;
}

/* This is required when building as a Node.js addon */
#ifndef ADDON_IS_HOST
#include <node.h>
extern "C" NODE_MODULE_EXPORT void
NODE_MODULE_INITIALIZER(Local<Object> exports, Local<Value> module, Local<Context> context) {
    /* Integrate uSockets with existing libuv loop */
    uWS::Loop::get(node::GetCurrentEventLoop(context->GetIsolate()));
    /* Register vanilla V8 addon */
    PerIsolateData *perIsolateData = Main(exports);

    /* We cannot rely on process.exit or process.beforeExit when it comes to WorkerThreads */
    node::AddEnvironmentCleanupHook(context->GetIsolate(), [](void *arg) {

        PerIsolateData *perIsolateData = (PerIsolateData *) arg;

        /* Freeing apps here, it could be done earlier but not sooner */
        perIsolateData->apps.clear();
        perIsolateData->sslApps.clear();
        /* Freeing the loop here means we give time for our timers to close, etc */
        uWS::Loop::get()->free();

        /* We can safely delete this since we no longer can call uWS.free */
        delete perIsolateData;

    }, perIsolateData);
}
#endif
