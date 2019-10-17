#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

/* uWS.App.ws('/pattern', behavior) */
template <typename APP>
void uWS_App_ws(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);
    /* This one is default constructed with defaults */
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
        UniquePersistent<Object> *socketPf;
    };

    /* Get the behavior object */
    if (args.Length() == 2) {
        Local<Object> behaviorObject = Local<Object>::Cast(args[1]);

        /* maxPayloadLength or default */
        MaybeLocal<Value> maybeMaxPayloadLength = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "maxPayloadLength"));
        if (!maybeMaxPayloadLength.IsEmpty() && !maybeMaxPayloadLength.ToLocalChecked()->IsUndefined()) {
            behavior.maxPayloadLength = maybeMaxPayloadLength.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* idleTimeout or default */
        MaybeLocal<Value> maybeIdleTimeout = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "idleTimeout"));
        if (!maybeIdleTimeout.IsEmpty() && !maybeIdleTimeout.ToLocalChecked()->IsUndefined()) {
            behavior.idleTimeout = maybeIdleTimeout.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* Compression or default, map from 0, 1, 2 to disabled, shared, dedicated. This is actually the enum */
        MaybeLocal<Value> maybeCompression = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "compression"));
        if (!maybeCompression.IsEmpty() && !maybeCompression.ToLocalChecked()->IsUndefined()) {
            behavior.compression = (uWS::CompressOptions) maybeCompression.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* maxBackpressure or default */
        MaybeLocal<Value> maybeMaxBackpressure = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "maxBackpressure"));
        if (!maybeMaxBackpressure.IsEmpty() && !maybeMaxBackpressure.ToLocalChecked()->IsUndefined()) {
            behavior.maxBackpressure = maybeMaxBackpressure.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
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

    /* Open handler is NOT optional for the wrapper */
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
        perSocketData->socketPf = new UniquePersistent<Object>;
        perSocketData->socketPf->Reset(isolate, wsObject);

        Local<Function> openLf = Local<Function>::New(isolate, openPf);
        if (!openLf->IsUndefined()) {
            Local<Value> argv[] = {wsObject, reqObject};
            openLf->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv).IsEmpty();
        }
    };

    /* Message handler is always optional */
    if (messagePf != Undefined(isolate)) {
        behavior.message = [messagePf = std::move(messagePf)](auto *ws, std::string_view message, uWS::OpCode opCode) {
            HandleScope hs(isolate);

            Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[3] = {Local<Object>::New(isolate, *(perSocketData->socketPf)),
                                    messageArrayBuffer,
                                    Boolean::New(isolate, opCode == uWS::OpCode::BINARY)};
            Local<Function>::New(isolate, messagePf)->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 3, argv).IsEmpty();

            /* Important: we clear the ArrayBuffer to make sure it is not invalidly used after return */
            messageArrayBuffer->Neuter();
        };
    }

    /* Drain handler is always optional */
    if (drainPf != Undefined(isolate)) {
        behavior.drain = [drainPf = std::move(drainPf)](auto *ws) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[1] = {Local<Object>::New(isolate, *(perSocketData->socketPf))
                                    };
            Local<Function>::New(isolate, drainPf)->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 1, argv).IsEmpty();
        };
    }

    /* These are not hooked in */
    behavior.ping = [](auto *ws) {

    };

    behavior.pong = [](auto *ws) {

    };

    /* Close handler is NOT optional for the wrapper */
    behavior.close = [closePf = std::move(closePf)](auto *ws, int code, std::string_view message) {
        HandleScope hs(isolate);

        Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer::New(isolate, (void *) message.data(), message.length());
        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        Local<Object> wsObject = Local<Object>::New(isolate, *(perSocketData->socketPf));

        /* Invalidate this wsObject */
        wsObject->SetAlignedPointerInInternalField(0, nullptr);

        /* Only call close handler if we have one set */
        Local<Function> closeLf = Local<Function>::New(isolate, closePf);
        if (!closeLf->IsUndefined()) {
            Local<Value> argv[3] = {wsObject, Integer::New(isolate, code), messageArrayBuffer};
            closeLf->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 3, argv).IsEmpty();
        }

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
        Local<Function>::New(isolate, *pf)->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv).IsEmpty();

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

    /* Require at least two arguments */
    if (args.Length() < 2) {
        /* Throw here */
        args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "App.listen requires port and callback")));
        return;
    }

    /* Callback is last */
    auto cb = [&args](auto *token) {
        /* Return a false boolean if listen failed */
        Local<Value> argv[] = {token ? Local<Value>::Cast(External::New(isolate, token)) : Local<Value>::Cast(Boolean::New(isolate, false))};
        Local<Function>::Cast(args[args.Length() - 1])->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 1, argv).IsEmpty();
    };

    /* Host is first, if present */
    std::string host;
    if (!args[0]->IsNumber()) {
        NativeString h(isolate, args[0]);
        if (h.isInvalid(args)) {
            return;
        }
        host = h.getString();
    }

    /* Port, options are in the middle, if present */
    std::vector<int> numbers;
    for (int i = std::min<int>(1, host.length()); i < args.Length() - 1; i++) {
        numbers.push_back(args[i]->Uint32Value(args.GetIsolate()->GetCurrentContext()).ToChecked());
    }

    /* We only use the most complete overload */
    app->listen(host, numbers.size() ? numbers[0] : 0,
                numbers.size() > 1 ? numbers[1] : 0, std::move(cb));

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_publish(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    NativeString topic(isolate, args[0]);
    NativeString message(isolate, args[1]);
    app->publish(topic.getString(), message.getString(), args[2]->BooleanValue(isolate->GetCurrentContext()).ToChecked() ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, args[3]->BooleanValue(isolate->GetCurrentContext()).ToChecked());
}

template <typename APP>
void uWS_App(const FunctionCallbackInfo<Value> &args) {
    Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);

    APP *app;

    /* Name differs based on type */
    if (std::is_same<APP, uWS::SSLApp>::value) {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLApp"));

        /* We fill these below */
        us_socket_context_options_t ssl_options = {};

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
                keyFileName = keyFileNameValue.getString();
                ssl_options.key_file_name = keyFileName.c_str();
            }

            /* Cert file name */
            NativeString certFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "cert_file_name")));
            if (certFileNameValue.isInvalid(args)) {
                return;
            }
            if (certFileNameValue.getString().length()) {
                certFileName = certFileNameValue.getString();
                ssl_options.cert_file_name = certFileName.c_str();
            }

            /* Passphrase */
            NativeString passphraseValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "passphrase")));
            if (passphraseValue.isInvalid(args)) {
                return;
            }
            if (passphraseValue.getString().length()) {
                passphrase = passphraseValue.getString();
                ssl_options.passphrase = passphrase.c_str();
            }

            /* DH params file name */
            NativeString dhParamsFileNameValue(isolate, Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "dh_params_file_name")));
            if (dhParamsFileNameValue.isInvalid(args)) {
                return;
            }
            if (dhParamsFileNameValue.getString().length()) {
                dhParamsFileName = dhParamsFileNameValue.getString();
                ssl_options.dh_params_file_name = dhParamsFileName.c_str();
            }

            /* ssl_prefer_low_memory_usage */
            ssl_options.ssl_prefer_low_memory_usage = Local<Object>::Cast(args[0])->Get(String::NewFromUtf8(isolate, "ssl_prefer_low_memory_usage"))->BooleanValue(isolate->GetCurrentContext()).ToChecked();
        }

        app = new APP(ssl_options);
    } else {
        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.App"));
        app = new APP;
    }

    /* Throw if we failed to construct the app */
    if (app->constructorFailed()) {
        delete app;
        args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "App construction failed")));
        return;
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
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "publish"), FunctionTemplate::New(isolate, uWS_App_publish<APP>));

    Local<Object> localApp = appTemplate->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    localApp->SetAlignedPointerInInternalField(0, app);

    /* Add this to our delete list */
    if constexpr (std::is_same<APP, uWS::SSLApp>::value) {
        sslApps.emplace_back(app);
    } else {
        apps.emplace_back(app);
    }

    args.GetReturnValue().Set(localApp);
}
