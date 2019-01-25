#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

/* uWS.App.ws('/pattern', behavior) */
template <typename APP>
void uWS_App_ws(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);
    typename APP::WebSocketBehavior behavior = {};

    NativeString pattern(args.GetIsolate(), args[0]);
    if (pattern.isInvalid(args)) {
        return;
    }

    UniquePersistent<Function> openPf;
    UniquePersistent<Function> messagePf;
    UniquePersistent<Function> drainPf;
    UniquePersistent<Function> closePf;

    struct PerSocketData {
        Persistent<Object> *socketPf;
    };

    /* Get the behavior object */
    if (args.Length() == 2) {
        Local<Object> behaviorObject = Local<Object>::Cast(args[1]);

        /* maxPayloadLength */
        behavior.maxPayloadLength = behaviorObject->Get(String::NewFromUtf8(isolate, "maxPayloadLength"))->Int32Value();

        /* idleTimeout */
        behavior.idleTimeout = behaviorObject->Get(String::NewFromUtf8(isolate, "idleTimeout"))->Int32Value();

        /* Compression, map from 0, 1, 2 to disabled, shared, dedicated */
        int compression = behaviorObject->Get(String::NewFromUtf8(isolate, "compression"))->Int32Value();
        if (compression == 0) {
            behavior.compression = uWS::CompressOptions::DISABLED;
        } else if (compression == 1) {
            behavior.compression = uWS::CompressOptions::SHARED_COMPRESSOR;
        } else if (compression == 2) {
            behavior.compression = uWS::CompressOptions::DEDICATED_COMPRESSOR;
        }

        /* Open */
        openPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "open"))));
        /* Message */
        messagePf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "message"))));
        /* Drain */
        drainPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "drain"))));
        /* Close */
        closePf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(String::NewFromUtf8(isolate, "close"))));
    }

    behavior.open = [openPf = std::move(openPf)](auto *ws, auto *req) {
        HandleScope hs(isolate);

        /* Create a new websocket object */
        Local<Object> wsObject = WebSocketWrapper::getWsInstance<APP>();
        wsObject->SetAlignedPointerInInternalField(0, ws);

        /* Create the HttpRequest wrapper */
        Local<Object> reqObject = HttpRequestWrapper::getReqInstance();
        reqObject->SetAlignedPointerInInternalField(0, req);

        /* Attach a new V8 object with pointer to us, to us */
        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        perSocketData->socketPf = new Persistent<Object>;
        perSocketData->socketPf->Reset(isolate, wsObject);

        Local<Value> argv[] = {wsObject, reqObject};
        Local<Function>::New(isolate, openPf)->Call(isolate->GetCurrentContext()->Global(), 2, argv);
    };

    behavior.message = [messagePf = std::move(messagePf)](auto *ws, std::string_view message, uWS::OpCode opCode) {
        HandleScope hs(isolate);

        Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());

        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        Local<Value> argv[3] = {Local<Object>::New(isolate, *(perSocketData->socketPf)),
                                messageArrayBuffer,
                                Boolean::New(isolate, opCode == uWS::OpCode::BINARY)};
        Local<Function>::New(isolate, messagePf)->Call(isolate->GetCurrentContext()->Global(), 3, argv);

        /* Important: we clear the ArrayBuffer to make sure it is not invalidly used after return */
        messageArrayBuffer->Neuter();
    };

    behavior.drain = [drainPf = std::move(drainPf)](auto *ws) {
        HandleScope hs(isolate);

        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        Local<Value> argv[1] = {Local<Object>::New(isolate, *(perSocketData->socketPf))
                                };
        Local<Function>::New(isolate, drainPf)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
    };

    behavior.ping = [](auto *ws) {

    };

    behavior.pong = [](auto *ws) {

    };

    behavior.close = [closePf = std::move(closePf)](auto *ws, int code, std::string_view message) {
        HandleScope hs(isolate);

        Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());
        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        Local<Object> wsObject = Local<Object>::New(isolate, *(perSocketData->socketPf));
        Local<Value> argv[3] = {wsObject,
                                Integer::New(isolate, code),
                                messageArrayBuffer};

        /* Invalidate this wsObject */
        wsObject->SetAlignedPointerInInternalField(0, nullptr);
        Local<Function>::New(isolate, closePf)->Call(isolate->GetCurrentContext()->Global(), 3, argv);

        delete perSocketData->socketPf;

        /* Again, here we clear the buffer to avoid strange bugs */
        messageArrayBuffer->Neuter();
    };

    app->template ws<PerSocketData>(std::string(pattern.getString()), std::move(behavior));

    /* Return this */
    args.GetReturnValue().Set(args.Holder());
}

/* This method wraps get, post and all http methods */
template <typename APP, typename F>
void uWS_App_get(F f, const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString pattern(args.GetIsolate(), args[0]);
    if (pattern.isInvalid(args)) {
        return;
    }

    /* todo: make it UniquePersistent */
    std::shared_ptr<Persistent<Function>> pf(new Persistent<Function>);
    pf->Reset(args.GetIsolate(), Local<Function>::Cast(args[1]));

    (app->*f)(std::string(pattern.getString()), [pf](auto *res, auto *req) {
        HandleScope hs(isolate);

        Local<Object> resObject = HttpResponseWrapper::getResInstance<APP>();
        resObject->SetAlignedPointerInInternalField(0, res);

        Local<Object> reqObject = HttpRequestWrapper::getReqInstance();
        reqObject->SetAlignedPointerInInternalField(0, req);

        Local<Value> argv[] = {resObject, reqObject};
        Local<Function>::New(isolate, *pf)->Call(isolate->GetCurrentContext()->Global(), 2, argv);

        /* Properly invalidate req */
        reqObject->SetAlignedPointerInInternalField(0, nullptr);

        /* µWS itself will terminate if not responded and not attached
         * onAborted handler, so we can assume it's done */
    });

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_listen(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    int port = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).ToChecked();

    app->listen(port, [&args](auto *token) {
        /* Return a false boolean if listen failed */
        Local<Value> argv[] = {token ? Local<Value>::Cast(External::New(isolate, token)) : Local<Value>::Cast(Boolean::New(isolate, false))};
        Local<Function>::Cast(args[1])->Call(isolate->GetCurrentContext()->Global(), 1, argv);
    });

    args.GetReturnValue().Set(args.Holder());
}

/* Mostly indended for debugging memory leaks */
template <typename APP>
void uWS_App_forcefully_free(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    delete app;
}

template <typename APP>
void uWS_App(const FunctionCallbackInfo<Value> &args) {
    Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);

    APP *app;

    /* Name differs based on type */
    if (std::is_same<APP, uWS::SSLApp>::value) {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLApp"));

        /* We fill these below */
        us_new_socket_context_options_t ssl_options = {};

        static std::string keyFileName;
        static std::string certFileName;
        static std::string passphrase;
        static std::string dhParamsFileName;

        /* Read the options object (SSL options) */
        if (args.Length() == 1) {
            /* Key file name */
            NativeString keyFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "key_file_name")));
            if (keyFileNameValue.isInvalid(args)) {
                return;
            }
            if (keyFileNameValue.getString().length()) {
                keyFileName.append(keyFileNameValue.getString());
                ssl_options.key_file_name = keyFileName.c_str();
            }

            /* Cert file name */
            NativeString certFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "cert_file_name")));
            if (certFileNameValue.isInvalid(args)) {
                return;
            }
            if (certFileNameValue.getString().length()) {
                certFileName.append(certFileNameValue.getString());
                ssl_options.cert_file_name = certFileName.c_str();
            }

            /* Passphrase */
            NativeString passphraseValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "passphrase")));
            if (passphraseValue.isInvalid(args)) {
                return;
            }
            if (passphraseValue.getString().length()) {
                passphrase.append(passphraseValue.getString());
                ssl_options.passphrase = passphrase.c_str();
            }

            /* DH params file name */
            NativeString dhParamsFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "dh_params_file_name")));
            if (dhParamsFileNameValue.isInvalid(args)) {
                return;
            }
            if (dhParamsFileNameValue.getString().length()) {
                dhParamsFileName.append(dhParamsFileNameValue.getString());
                ssl_options.dh_params_file_name = dhParamsFileName.c_str();
            }
        }

        app = new APP(ssl_options);
    } else {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.App"));
        app = new APP;
    }

    appTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    /* All the http methods */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "get"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::get, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "post"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::post, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "options"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::options, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "del"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::del, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "patch"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::patch, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "put"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::put, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "head"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::head, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "connect"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::connect, args);
    }));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "trace"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::trace, args);
    }));

    /* Any http method */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "any"), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::any, args);
    }));

    /* ws, listen */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "ws"), FunctionTemplate::New(isolate, uWS_App_ws<APP>));
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen"), FunctionTemplate::New(isolate, uWS_App_listen<APP>));

    /* forcefully_free is unsafe for end-users to use, but nice to track memory leaks with ASAN */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "forcefully_free"), FunctionTemplate::New(isolate, uWS_App_forcefully_free<APP>));

    Local<Object> localApp = appTemplate->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    localApp->SetAlignedPointerInInternalField(0, app);

    args.GetReturnValue().Set(localApp);
}
