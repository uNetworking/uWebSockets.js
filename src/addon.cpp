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

// missing features to wrap:
// App.post and all other methods - requires good templating?
// req.getParam(int)
// req.getUrl()
// res.onData(JS function)
// res.write
// res.tryEnd
// req.onAbort ?
// ws.getBufferedAmount()
// ws.close(lalala)
// ws.?
// test so that we pass Autobahn with compression/without compression with SSL/without SSL
// split addon in different separate headers

/* We are only allowed to depend on µWS and V8 in this layer. */
#include "App.h"
#include <v8.h>
using namespace v8;
Isolate *isolate;

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

// also wrap all of this in some common struct
Persistent<Object> wsTemplate[2];

/* WebSocket send */

template <bool SSL>
void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
    NativeString nativeString(args.GetIsolate(), args[0]);

    bool isBinary = args[1]->Int32Value();

    bool ok = ((uWS::WebSocket<SSL, true> *) args.Holder()->GetAlignedPointerFromInternalField(0))->send(
                std::string_view(nativeString.getData(), nativeString.getLength()), isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT
                );

    args.GetReturnValue().Set(Boolean::New(isolate, ok));
}

template <bool SSL>
void initWsTemplate() {
    Local<FunctionTemplate> wsTemplateLocal = FunctionTemplate::New(isolate);
    if (SSL) {
        wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLWebSocket"));
    } else {
        wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.WebSocket"));
    }
    wsTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send"), FunctionTemplate::New(isolate, uWS_WebSocket_send<SSL>));

    Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    wsTemplate[SSL].Reset(isolate, wsObjectLocal);
}

template <class APP>
Local<Object> getWsInstance() {
    return Local<Object>::New(isolate, wsTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
}

/*
template <bool SSL>
struct HttpResponseWrapper {

    HttpResponseWrapper() {

    }



};*/

// this whole template thing could be one struct with members to order tihngs better
Persistent<Object> resTemplate[2];

template <bool SSL>
void res_end(const FunctionCallbackInfo<Value> &args) {
    NativeString data(args.GetIsolate(), args[0]);
    ((uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0))->end(std::string_view(data.getData(), data.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

template <bool SSL>
void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
    NativeString header(args.GetIsolate(), args[0]);
    NativeString value(args.GetIsolate(), args[1]);
    ((uWS::HttpResponse<SSL> *) args.Holder()->GetAlignedPointerFromInternalField(0))->writeHeader(std::string_view(header.getData(), header.getLength()),
                                                                                                     std::string_view(value.getData(), value.getLength()));

    args.GetReturnValue().Set(args.Holder());
}

template <bool SSL>
void initResTemplate() {
    Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
    if (SSL) {
        resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLHttpResponse"));
    } else {
        resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
    }
    resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end<SSL>));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader<SSL>));

    Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    resTemplate[SSL].Reset(isolate, resObjectLocal);
}

template <class APP>
Local<Object> getResInstance() {
    return Local<Object>::New(isolate, resTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
}

/* The only thing this req needs is getHeader and similar, getParameter, getUrl and so on */
Persistent<Object> reqTemplate;

void req_getHeader(const FunctionCallbackInfo<Value> &args) {
    NativeString data(args.GetIsolate(), args[0]);
    char *buf = data.getData(); int length = data.getLength();

    std::string_view header = ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0))->getHeader(std::string_view(buf, length));

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
}

/* uWS.App.ws('/pattern', options) */
template <typename APP>
void uWS_App_ws(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    // pattern
    NativeString nativeString(args.GetIsolate(), args[0]);

    // todo: small leak here, should be unique_ptrs moved in
    Persistent<Function> *openPf = new Persistent<Function>();
    Persistent<Function> *messagePf = new Persistent<Function>();
    Persistent<Function> *drainPf = new Persistent<Function>();
    Persistent<Function> *closePf = new Persistent<Function>();

    int maxPayloadLength = 0;

    /* For now, let's have 0, 1, 2 be from nothing to shared, to dedicated */
    int compression = 0;
    uWS::CompressOptions mappedCompression = uWS::CompressOptions::DISABLED;

    struct PerSocketData {
        Persistent<Object> *socketPf;
    };

    /* Get the behavior object */
    if (args.Length() == 2) {
        Local<Object> behaviorObject = Local<Object>::Cast(args[1]);

        /* maxPayloadLength */
        maxPayloadLength = behaviorObject->Get(String::NewFromUtf8(isolate, "maxPayloadLength"))->Int32Value();

        /* Compression */
        compression = behaviorObject->Get(String::NewFromUtf8(isolate, "compression"))->Int32Value();

        /* Open */
        openPf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "open"))));
        /* Message */
        messagePf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "message"))));
        /* Drain */
        drainPf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "drain"))));
        /* Close */
        closePf->Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "close"))));
    }

    /* Map compression options from integer values */
    if (compression == 1) {
        mappedCompression = uWS::CompressOptions::SHARED_COMPRESSOR;
    } else if (compression == 2) {
        mappedCompression = uWS::CompressOptions::DEDICATED_COMPRESSOR;
    }

    app->template ws<PerSocketData>(std::string(nativeString.getData(), nativeString.getLength()), {
        /* idleTimeout */
        .compression = mappedCompression,
        .maxPayloadLength = maxPayloadLength,
        /* Handlers */
        .open = [openPf](auto *ws, auto *req) {
            HandleScope hs(isolate);

            /* Create a new websocket object */
            Local<Object> wsObject = getWsInstance<APP>();
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

            Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[3] = {Local<Object>::New(isolate, *(perSocketData->socketPf)),
                                    /*ArrayBuffer::New(isolate, (void *) message.data(), message.length())*/ messageArrayBuffer,
                                    Boolean::New(isolate, opCode == uWS::OpCode::BINARY)
                                   };
            Local<Function>::New(isolate, *messagePf)->Call(isolate->GetCurrentContext()->Global(), 3, argv);

            /* Important: we clear the ArrayBuffer to make sure it is not invalidly used after return */
            messageArrayBuffer->Neuter();
        },
        .drain = [drainPf](auto *ws) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[1] = {Local<Object>::New(isolate, *(perSocketData->socketPf))
                                   };
            Local<Function>::New(isolate, *drainPf)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
        },
        .ping = [](auto *ws) {

        },
        .pong = [](auto *ws) {

        },
        .close = [closePf](auto *ws, int code, std::string_view message) {
            HandleScope hs(isolate);

            Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[3] = {Local<Object>::New(isolate, *(perSocketData->socketPf)),
                                    Integer::New(isolate, code),
                                    messageArrayBuffer
                                   };
            Local<Function>::New(isolate, *closePf)->Call(isolate->GetCurrentContext()->Global(), 3, argv);

            /* Again, here we clear the buffer to avoid strange bugs */
            messageArrayBuffer->Neuter();
        }
    });

    /* Return this */
    args.GetReturnValue().Set(args.Holder());
}

// todo: all other methods, in particular post!
template <typename APP>
void uWS_App_get(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString nativeString(args.GetIsolate(), args[0]);

    Persistent<Function> *pf = new Persistent<Function>();
    pf->Reset(args.GetIsolate(), Local<Function>::Cast(args[1]));

    //Persistent<Function, CopyablePersistentTraits<Function>> p(isolate, Local<Function>::Cast(args[1]));

    app->get(std::string(nativeString.getData(), nativeString.getLength()), [pf](auto *res, auto *req) {
        HandleScope hs(isolate);

        Local<Object> resObject = getResInstance<APP>();
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


    /* Most used functions will be get, post, ws, listen */

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
    initWsTemplate<0>();
    initWsTemplate<1>();

    /* Initialize SSL and non-SSL templates */
    initResTemplate<0>();
    initResTemplate<1>();

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
