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
    us_listen_socket_close((struct us_listen_socket *) External::Cast(*args[0])->Value());
}

void Main(Local<Object> exports) {
    /* I guess we store this statically */
    isolate = exports->GetIsolate();

    /* We want this so that we can redefine process.nextTick to using the V8 native microtask queue */
    // todo: setting this might be crashing nodejs?
    isolate->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);

    /* Integrate with existing libuv loop, we just pass a boolean basically */
    uWS::Loop::get((void *) 1);

    // instead, for now we call this manually like before:
    /*uWS::Loop::get()->setPostHandler([](uWS::Loop *) {
        isolate->RunMicrotasks();
    });

    uWS::Loop::get()->setPreHandler([](uWS::Loop *) {
        isolate->RunMicrotasks();
    });*/

    /* uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "App"), FunctionTemplate::New(isolate, uWS_App<uWS::App>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "SSLApp"), FunctionTemplate::New(isolate, uWS_App<uWS::SSLApp>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "free"), FunctionTemplate::New(isolate, uWS_free)->GetFunction());

    /* Expose some µSockets functions directly under uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "us_listen_socket_close"), FunctionTemplate::New(isolate, uWS_us_listen_socket_close)->GetFunction());

    /* Compression enum */
    exports->Set(String::NewFromUtf8(isolate, "DISABLED"), Integer::NewFromUnsigned(isolate, uWS::DISABLED));
    exports->Set(String::NewFromUtf8(isolate, "SHARED_COMPRESSOR"), Integer::NewFromUnsigned(isolate, uWS::SHARED_COMPRESSOR));
    exports->Set(String::NewFromUtf8(isolate, "DEDICATED_COMPRESSOR"), Integer::NewFromUnsigned(isolate, uWS::DEDICATED_COMPRESSOR));

    /* Listen enum */
    exports->Set(String::NewFromUtf8(isolate, "OPTION_DO_NOT_REUSE_PORT"), Integer::NewFromUnsigned(isolate, uWS::OPTION_DO_NOT_REUSE_PORT));

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
