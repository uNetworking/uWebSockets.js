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

#include "Utilities.h"
#include "WebSocketWrapper.h"
#include "HttpResponseWrapper.h"
#include "HttpRequestWrapper.h"
#include "AppWrapper.h"

/* Todo: Apps should be freed once the GC says so BUT ALWAYS before freeing the loop */

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

void Main(Local<Object> exports) {

    /* We pass isolate everywhere */
    Isolate *isolate = exports->GetIsolate();

#ifndef PERFORM_LIKE_GARBAGE
    /* We want this so that we can redefine process.nextTick to using the V8 native microtask queue */
    /* Settings this crashes Node.js while debugging with breakpoints */
    isolate->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);
#endif

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

    /* Expose some µSockets functions directly under uWS namespace */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "us_listen_socket_close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_us_listen_socket_close)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()).ToChecked();

    /* Compression enum */
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DISABLED", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DISABLED)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "SHARED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::SHARED_COMPRESSOR)).ToChecked();
    exports->Set(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR", NewStringType::kNormal).ToLocalChecked(), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR)).ToChecked();

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