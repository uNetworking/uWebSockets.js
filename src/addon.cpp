/*
 * Copyright 2018 Alex Hultman and contributors.

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
using namespace v8;
Isolate *isolate;
Persistent<Object> resTemplate;
Persistent<Object> reqTemplate;
Persistent<Object> wsTemplate;

#include <iostream>
#include <vector>
#include <type_traits>
std::vector<Persistent<Function, CopyablePersistentTraits<Function>>> nextTickQueue;

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
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
            ArrayBuffer::Contents contents = arrayBufferView->Buffer()->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            ArrayBuffer::Contents contents = arrayBuffer->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else {
            static char empty[] = "";
            data = empty;
            length = 0;
        }
    }

    char *getData() {return data;}
    size_t getLength() {return length;}
    ~NativeString() {
        if (utf8Value) {
            utf8Value->~Utf8Value();
        }
    }
};

void res_end(const FunctionCallbackInfo<Value> &args) {

    // you might want to do extra work here to swap to tryEnd if passed a Buffer?
    // or always use tryEnd and simply grab the object as persistent?

    NativeString data(args.GetIsolate(), args[0]);
    ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->end(std::string_view(data.getData(), data.getLength()));

    // Return this
    args.GetReturnValue().Set(args.Holder());
}

void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
    // get string
    NativeString header(args.GetIsolate(), args[0]);
    NativeString value(args.GetIsolate(), args[1]);

    ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->writeHeader(std::string_view(header.getData(), header.getLength()),
                                                                                                     std::string_view(value.getData(), value.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

void req_getHeader(const FunctionCallbackInfo<Value> &args) {
    // get string
    NativeString data(args.GetIsolate(), args[0]);
    char *buf = data.getData(); int length = data.getLength();

    std::string_view header = ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0))->getHeader(std::string_view(buf, length));

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
}

/* WebSocket send */
// not properly templated, just like the httpsockets are not!
void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
    uWS::WebSocket<false, true> *ws = (uWS::WebSocket<false, true> *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString nativeString(args.GetIsolate(), args[0]);

    //std::cout << std::string_view(nativeString.getData(), nativeString.getLength()) << std::endl;

    ws->send(std::string_view(nativeString.getData(), nativeString.getLength()), uWS::OpCode::TEXT);
}

/* uWS.App.ws('/pattern', options) */
template <typename APP>
void uWS_App_ws(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString nativeString(args.GetIsolate(), args[0]);

    Persistent<Function> *openPf = new Persistent<Function>();
    Persistent<Function> *messagePf = new Persistent<Function>();

    struct PerSocketData {
        Persistent<Object> *socketPf;
    };

    /* Get the behavior object */
    if (args.Length() == 2) {
        Local<Object> behaviorObject = Local<Object>::Cast(args[1]);

        /* Open */
        openPf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "open"))));
        /* Message */
        messagePf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "message"))));
    }

    app->template ws<PerSocketData>(std::string(nativeString.getData(), nativeString.getLength()), {

        /*.compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024,*/
        .open = [openPf](auto *ws, auto *req) {
           HandleScope hs(isolate);

           /* Create a new websocket object */
           Local<Object> wsObject = Local<Object>::New(isolate, wsTemplate)->Clone();
           wsObject->SetAlignedPointerInInternalField(0, ws);

           /* Attach a new V8 object with pointer to us, to us */
           PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
           perSocketData->socketPf = new Persistent<Object>;
           perSocketData->socketPf->Reset(isolate, wsObject);

           Local<Value> argv[] = {wsObject};
           Local<Function>::New(isolate, *openPf)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
        },
        .message = [messagePf](auto *ws, std::string_view message, uWS::OpCode opCode) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[2] = {Local<Object>::New(isolate, *(perSocketData->socketPf)), ArrayBuffer::New(isolate, (void *) message.data(), message.length())}; // ws, message, opCode
            Local<Function>::New(isolate, *messagePf)->Call(isolate->GetCurrentContext()->Global(), 2, argv);
        }/*
        .drain = []() {},
        .ping = []() {},
        .pong = []() {},
        .close = []() {}*/
    });

    // Return this
    args.GetReturnValue().Set(args.Holder());
}

// todo: all other methods
template <typename APP>
void uWS_App_get(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString nativeString(args.GetIsolate(), args[0]);

    Persistent<Function> *pf = new Persistent<Function>();
    pf->Reset(args.GetIsolate(), Local<Function>::Cast(args[1]));

    //Persistent<Function, CopyablePersistentTraits<Function>> p(isolate, Local<Function>::Cast(args[1]));

    app->get(std::string(nativeString.getData(), nativeString.getLength()), [pf](auto *res, auto *req) {
        HandleScope hs(isolate);

        Local<Object> resObject = Local<Object>::New(isolate, resTemplate)->Clone();
        resObject->SetAlignedPointerInInternalField(0, res);

        Local<Object> reqObject = Local<Object>::New(isolate, reqTemplate)->Clone();
        reqObject->SetAlignedPointerInInternalField(0, req);

        Local<Value> argv[] = {resObject, reqObject};
        Local<Function>::New(isolate, *pf)->Call(isolate->GetCurrentContext()->Global(), 2, argv);
    });

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_listen(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    int port = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).ToChecked();

    app->listen(port, [&args](auto *token) {
        Local<Value> argv[] = {Boolean::New(isolate, token)};
        Local<Function>::Cast(args[1])->Call(isolate->GetCurrentContext()->Global(), 1, argv);
    });

    // Return this
    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App(const FunctionCallbackInfo<Value> &args) {
    Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);

    APP *app;

    /* Name differs based on type */
    if (std::is_same<APP, uWS::SSLApp>::value) {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLApp"));

        /* We fill these below */
        us_ssl_socket_context_options ssl_options = {};

        static std::string keyFileName;
        static std::string certFileName;
        static std::string passphrase;

        /* Read the options object (SSL options) */
        if (args.Length() == 1) {
            /* Key file name */
            NativeString keyFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "key_file_name")));
            if (keyFileNameValue.getLength()) {
                keyFileName.append(keyFileNameValue.getData(), keyFileNameValue.getLength());
                ssl_options.key_file_name = keyFileName.c_str();
            }

            /* Cert file name */
            NativeString certFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "cert_file_name")));
            if (certFileNameValue.getLength()) {
                certFileName.append(certFileNameValue.getData(), certFileNameValue.getLength());
                ssl_options.cert_file_name = certFileName.c_str();
            }

            /* Passphrase */
            NativeString passphraseValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "passphrase")));
            if (passphraseValue.getLength()) {
                passphrase.append(passphraseValue.getData(), passphraseValue.getLength());
                ssl_options.passphrase = passphrase.c_str();
            }
        }

        app = new APP(ssl_options);
    } else {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.App"));
        app = new APP;
    }

    appTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    // Get and all the Http methods
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "get"), FunctionTemplate::New(isolate, uWS_App_get<APP>));

    // Ws
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "ws"), FunctionTemplate::New(isolate, uWS_App_ws<APP>));

    // Listen
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen"), FunctionTemplate::New(isolate, uWS_App_listen<APP>));

    // Instantiate and set intenal pointer
    Local<Object> localApp = appTemplate->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    // Delete this boy
    localApp->SetAlignedPointerInInternalField(0, app);

    // Return an instance of this shit
    args.GetReturnValue().Set(localApp);
}

// we need to override process.nextTick to avoid horrible loss of performance by Node.js
void nextTick(const FunctionCallbackInfo<Value> &args) {
    nextTickQueue.push_back(Persistent<Function, CopyablePersistentTraits<Function>>(isolate, Local<Function>::Cast(args[0])));
}

void emptyNextTickQueue(Isolate *isolate) {
    if (nextTickQueue.size()) {
        HandleScope hs(isolate);

        for (Persistent<Function, CopyablePersistentTraits<Function>> &f : nextTickQueue) {
            Local<Function>::New(isolate, f)->Call(isolate->GetCurrentContext()->Global(), 0, nullptr);
            f.Reset();
        }

        nextTickQueue.clear();
    }
}

void Main(Local<Object> exports) {

    isolate = exports->GetIsolate();

    /* Register our own nextTick handler */
    /* We should probably also do this in pre, just to be sure */
    uWS::Loop::defaultLoop()->setPostHandler([](uWS::Loop *) {
        emptyNextTickQueue(isolate);
    });

    /* Hook up our timers */
    us_loop_integrate((us_loop *) uWS::Loop::defaultLoop());

    /*reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
    reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);*/

    /* uWS namespace */
    exports->Set(String::NewFromUtf8(isolate, "App"), FunctionTemplate::New(isolate, uWS_App<uWS::App>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "SSLApp"), FunctionTemplate::New(isolate, uWS_App<uWS::SSLApp>)->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "nextTick"), FunctionTemplate::New(isolate, nextTick)->GetFunction());

    /* The template for websockets */
    Local<FunctionTemplate> wsTemplateLocal = FunctionTemplate::New(isolate);
    wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.WebSocket"));
    wsTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send"), FunctionTemplate::New(isolate, uWS_WebSocket_send));

    Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    wsTemplate.Reset(isolate, wsObjectLocal);

    // HttpResponse template (not templated)
    Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
    resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
    resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader));

    Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    resTemplate.Reset(isolate, resObjectLocal);

    // Request template (do we need to clone this?)
    Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
    reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
    reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));

    Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    reqTemplate.Reset(isolate, reqObjectLocal);
}

/* This is required when building as a Node.js addon */
#ifndef ADDON_IS_HOST
#include <node.h>
NODE_MODULE(uWS, Main)
#endif
