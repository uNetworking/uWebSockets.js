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
#include <v8.h>
#include <iostream>
#include <vector>
#include <type_traits>
using namespace v8;

/* These two are definitely static */
Isolate *isolate;
bool valid = true;

/* We hold all apps until free */
std::vector<std::unique_ptr<uWS::App>> apps;
std::vector<std::unique_ptr<uWS::SSLApp>> sslApps;

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

#include "Utilities.h"
#include "WebSocketWrapper.h"
#include "HttpResponseWrapper.h"
#include "HttpRequestWrapper.h"
#include "AppWrapper.h"

/* Todo: Apps should be freed once the GC says so BUT ALWAYS before freeing the loop */

/* This has to be called in beforeExit, but exit also seems okay */
void uWS_free(const FunctionCallbackInfo<Value> &args) {
    if (valid) {
        /* Freeing apps here, it could be done earlier but not sooner */
        apps.clear();
        sslApps.clear();
        /* Freeing the loop here means we give time for our timers to close, etc */
        uWS::Loop::get()->free();
        valid = false;
    }
}

/* todo: Put this function and all inits of it in its own header */
void uWS_us_listen_socket_close(const FunctionCallbackInfo<Value> &args) {
    // this should take int ssl first
    us_listen_socket_close(0, (struct us_listen_socket_t *) External::Cast(*args[0])->Value());
}

#include <uv.h>

void Main(Local<Object> exports) {
    /* I guess we store this statically */
    isolate = exports->GetIsolate();

    /* We want this so that we can redefine process.nextTick to using the V8 native microtask queue */
    // todo: setting this might be crashing nodejs?
    isolate->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);

    /* Integrate with existing libuv loop, we just pass a boolean basically */
    uWS::Loop::get(uv_default_loop());

    /* uWS namespace */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "App", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App<uWS::App>)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "SSLApp", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App<uWS::SSLApp>)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "free", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_free)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Expose some µSockets functions directly under uWS namespace */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "us_listen_socket_close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_us_listen_socket_close)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Compression enum */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DISABLED", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DISABLED)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "SHARED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::SHARED_COMPRESSOR)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR)).ToChecked();

    /* Listen options */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "LIBUS_LISTEN_EXCLUSIVE_PORT", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, LIBUS_LISTEN_EXCLUSIVE_PORT)).ToChecked();

    /* The template for websockets */
    WebSocketWrapper::initWsTemplate<0>();
    WebSocketWrapper::initWsTemplate<1>();

    /* Initialize SSL and non-SSL templates */
    HttpResponseWrapper::initResTemplate<0>();
    HttpResponseWrapper::initResTemplate<1>();

    /* Init a shared request object */
    HttpRequestWrapper::initReqTemplate();
}

/* This is required when building as a Node.js addon */
#ifndef ADDON_IS_HOST
#include <node.h>
NODE_MODULE(uWS, Main)
#endif
