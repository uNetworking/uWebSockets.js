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

/* Warning: having nextTickQueue items at loop fallthrough is not allowed.
 * Process will crash/hang due to destruction of V8 resouces after V8 itself
 * has been destroyed. Either enforce nextTick calls to keep the loop rolling
 * via for instance setTimeout or setImmediate, or make sure to drain completely
 * the queue at process.on('beforeExit'). */
std::vector<UniquePersistent<Function>> nextTickQueue;
Isolate *isolate;

#include "Utilities.h"
#include "WebSocketWrapper.h"
#include "HttpResponseWrapper.h"
#include "HttpRequestWrapper.h"
#include "AppWrapper.h"

/* We are not compatible with Node.js nextTick for performance (and standalone) reasons */
void nextTick(const FunctionCallbackInfo<Value> &args) {
    nextTickQueue.emplace_back(UniquePersistent<Function>(isolate, Local<Function>::Cast(args[0])));
}

/* Used for debugging */
void print(const FunctionCallbackInfo<Value> &args) {
    NativeString nativeString(isolate, args[0]);
    std::cout << nativeString.getString() << std::endl;
}

/* Does not guarantee empty queue because of recursive nextTick calls.
 * Should return int queueSize after calling queued items, so that
 * proper while(processNextTickQueueImpl()) can be done */
int processNextTickQueueImpl(Isolate *isolate) {
    if (nextTickQueue.size()) {
        /* Swap queues for recursive calls */
        std::vector<UniquePersistent<Function>> currentNextTickQueue = std::move(nextTickQueue);

        HandleScope hs(isolate);
        for (UniquePersistent<Function> &f : currentNextTickQueue) {
            Local<Function>::New(isolate, f)->Call(isolate->GetCurrentContext()->Global(), 0, nullptr);
        }
    }

    return nextTickQueue.size();
}

/* It is possible to call this at process.beforeExit until it returns 0. */
void processNextTickQueue(const FunctionCallbackInfo<Value> &args) {
    args.GetReturnValue().Set(Integer::New(isolate, processNextTickQueueImpl(isolate)));
}

/* todo: Put this function and all inits of it in its own header */
void uWS_us_listen_socket_close(const FunctionCallbackInfo<Value> &args) {
    us_listen_socket_close((struct us_listen_socket *) External::Cast(*args[0])->Value());
}

void Main(Local<Object> exports) {
    /* I guess we store this statically */
    isolate = exports->GetIsolate();

    /* Register our own nextTick handlers */
    uWS::Loop::defaultLoop()->setPostHandler([](uWS::Loop *) {
        processNextTickQueueImpl(isolate);
    });

    /* We also do need it on pre */
    uWS::Loop::defaultLoop()->setPreHandler([](uWS::Loop *) {
        processNextTickQueueImpl(isolate);
    });

    /* Hook up our timers */
    us_loop_integrate((us_loop *) uWS::Loop::defaultLoop());

    /* uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "App"), FunctionTemplate::New(isolate, uWS_App<uWS::App>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "SSLApp"), FunctionTemplate::New(isolate, uWS_App<uWS::SSLApp>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "nextTick"), FunctionTemplate::New(isolate, nextTick)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "processNextTickQueue"), FunctionTemplate::New(isolate, processNextTickQueue)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "print"), FunctionTemplate::New(isolate, print)->GetFunction());

    /* Expose some µSockets functions directly under uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "us_listen_socket_close"), FunctionTemplate::New(isolate, uWS_us_listen_socket_close)->GetFunction());

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
