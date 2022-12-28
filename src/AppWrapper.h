/*
 * Authored by Alex Hultman, 2018-2021.
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

#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

/* uWS.App.ws('/pattern', behavior) */
template <typename APP>
void uWS_App_ws(const FunctionCallbackInfo<Value> &args) {

    /* pattern, behavior */
    if (missingArguments(2, args)) {
        return;
    }

    Isolate *isolate = args.GetIsolate();

    PerContextData *perContextData = (PerContextData *) Local<External>::Cast(args.Data())->Value();

    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);
    /* This one is default constructed with defaults */
    typename APP::template WebSocketBehavior<PerSocketData> behavior = {};

    NativeString pattern(args.GetIsolate(), args[0]);
    if (pattern.isInvalid(args)) {
        return;
    }

    UniquePersistent<Function> upgradePf;
    UniquePersistent<Function> openPf;
    UniquePersistent<Function> messagePf;
    UniquePersistent<Function> drainPf;
    UniquePersistent<Function> closePf;
    UniquePersistent<Function> pingPf;
    UniquePersistent<Function> pongPf;
    UniquePersistent<Function> subscriptionPf;

    /* Get the behavior object */
    if (args.Length() == 2) {
        Local<Object> behaviorObject = Local<Object>::Cast(args[1]);

        /* maxPayloadLength or default */
        MaybeLocal<Value> maybeMaxPayloadLength = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "maxPayloadLength", NewStringType::kNormal).ToLocalChecked());
        if (!maybeMaxPayloadLength.IsEmpty() && !maybeMaxPayloadLength.ToLocalChecked()->IsUndefined()) {
            behavior.maxPayloadLength = maybeMaxPayloadLength.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* idleTimeout or default */
        MaybeLocal<Value> maybeIdleTimeout = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "idleTimeout", NewStringType::kNormal).ToLocalChecked());
        if (!maybeIdleTimeout.IsEmpty() && !maybeIdleTimeout.ToLocalChecked()->IsUndefined()) {
            behavior.idleTimeout = maybeIdleTimeout.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* maxLifetime or default */
        MaybeLocal<Value> maybeMaxLifetime = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "maxLifetime", NewStringType::kNormal).ToLocalChecked());
        if (!maybeMaxLifetime.IsEmpty() && !maybeMaxLifetime.ToLocalChecked()->IsUndefined()) {
            behavior.maxLifetime = maybeMaxLifetime.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* closeOnBackpressureLimit or default */
        MaybeLocal<Value> maybeCloseOnBackpressureLimit = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "closeOnBackpressureLimit", NewStringType::kNormal).ToLocalChecked());
        if (!maybeCloseOnBackpressureLimit.IsEmpty() && !maybeCloseOnBackpressureLimit.ToLocalChecked()->IsUndefined()) {
            behavior.closeOnBackpressureLimit = maybeCloseOnBackpressureLimit.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* sendPingsAutomatically or default */
        MaybeLocal<Value> maybeSendPingsAutomatically = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "sendPingsAutomatically", NewStringType::kNormal).ToLocalChecked());
        if (!maybeSendPingsAutomatically.IsEmpty() && !maybeSendPingsAutomatically.ToLocalChecked()->IsUndefined()) {
            behavior.sendPingsAutomatically = maybeSendPingsAutomatically.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* Compression or default, map from 0, 1, 2 to disabled, shared, dedicated. This is actually the enum */
        MaybeLocal<Value> maybeCompression = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "compression", NewStringType::kNormal).ToLocalChecked());
        if (!maybeCompression.IsEmpty() && !maybeCompression.ToLocalChecked()->IsUndefined()) {
            behavior.compression = (uWS::CompressOptions) maybeCompression.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* maxBackpressure or default */
        MaybeLocal<Value> maybeMaxBackpressure = behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "maxBackpressure", NewStringType::kNormal).ToLocalChecked());
        if (!maybeMaxBackpressure.IsEmpty() && !maybeMaxBackpressure.ToLocalChecked()->IsUndefined()) {
            behavior.maxBackpressure = maybeMaxBackpressure.ToLocalChecked()->Int32Value(isolate->GetCurrentContext()).ToChecked();
        }

        /* Upgrade */
        upgradePf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "upgrade", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Open */
        openPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "open", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Message */
        messagePf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "message", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Drain */
        drainPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "drain", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Close */
        closePf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Ping */
        pingPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "ping", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
        /* Pong */
        pongPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "pong", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));
    	/* Subscription */
        subscriptionPf.Reset(args.GetIsolate(), Local<Function>::Cast(behaviorObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "subscription", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()));

    }

    /* Upgrade handler is always optional */
    if (upgradePf != Undefined(isolate)) {
        behavior.upgrade = [upgradePf = std::move(upgradePf), perContextData](auto *res, auto *req, auto *context) {
            Isolate *isolate = perContextData->isolate;
            HandleScope hs(isolate);

            Local<Function> upgradeLf = Local<Function>::New(isolate, upgradePf);
            Local<Object> resObject = perContextData->resTemplate[getAppTypeIndex<APP>()].Get(isolate)->Clone();
            resObject->SetAlignedPointerInInternalField(0, res);

            Local<Object> reqObject = perContextData->reqTemplate[std::is_same<APP, uWS::H3App>::value].Get(isolate)->Clone();
            reqObject->SetAlignedPointerInInternalField(0, req);

            Local<Value> argv[3] = {resObject, reqObject, External::New(isolate, (void *) context)};
            CallJS(isolate, upgradeLf, 3, argv);

            /* Properly invalidate req */
            reqObject->SetAlignedPointerInInternalField(0, nullptr);

            /* µWS itself will terminate if not responded and not attached
            * onAborted handler, so we can assume it's done */
        };
    }

    /* Open handler is NOT optional for the wrapper */
    behavior.open = [openPf = std::move(openPf), perContextData](auto *ws) {
        Isolate *isolate = perContextData->isolate;
        HandleScope hs(isolate);

        /* Create a new websocket object */
        Local<Object> wsObject = perContextData->wsTemplate[getAppTypeIndex<APP>()].Get(isolate)->Clone();
        wsObject->SetAlignedPointerInInternalField(0, ws);

        /* Retrieve temporary userData object */
        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();

        /* Copy entires from userData, only if we have it set (not the case for default constructor) */
        if (!perSocketData->socketPf.IsEmpty()) {
            /* socketPf points to a stack allocated UniquePersistent, or nullptr, at this point */
            Local<Object> userData = Local<Object>::New(isolate, perSocketData->socketPf);

            /* Merge userData and wsObject; this code is exceedingly horrible */
            Local<Array> keys;
            if (userData->GetOwnPropertyNames(isolate->GetCurrentContext()).ToLocal(&keys)) {
                for (int i = 0; i < keys->Length(); i++) {
                    wsObject->Set(isolate->GetCurrentContext(),
                        keys->Get(isolate->GetCurrentContext(), i).ToLocalChecked(),
                        userData->Get(isolate->GetCurrentContext(), keys->Get(isolate->GetCurrentContext(), i).ToLocalChecked()).ToLocalChecked()
                        ).ToChecked();
                }
            }
        }

        /* Attach a new V8 object with pointer to us, to it */
        perSocketData->socketPf.Reset(isolate, wsObject);

        Local<Function> openLf = Local<Function>::New(isolate, openPf);
        if (!openLf->IsUndefined()) {
            Local<Value> argv[] = {wsObject};
            CallJS(isolate, openLf, 1, argv);
        }
    };

    /* Message handler is always optional */
    if (messagePf != Undefined(isolate)) {
        behavior.message = [messagePf = std::move(messagePf), isolate](auto *ws, std::string_view message, uWS::OpCode opCode) {
            HandleScope hs(isolate);

            Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer_New(isolate, (void *) message.data(), message.length());

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[3] = {Local<Object>::New(isolate, perSocketData->socketPf),
                                    messageArrayBuffer,
                                    Boolean::New(isolate, opCode == uWS::OpCode::BINARY)};

            CallJS(isolate, Local<Function>::New(isolate, messagePf), 3, argv);

            /* Important: we clear the ArrayBuffer to make sure it is not invalidly used after return */
            messageArrayBuffer->Detach();
        };
    }

    /* Drain handler is always optional */
    if (drainPf != Undefined(isolate)) {
        behavior.drain = [drainPf = std::move(drainPf), isolate](auto *ws) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[1] = {Local<Object>::New(isolate, perSocketData->socketPf)
                                    };
            CallJS(isolate, Local<Function>::New(isolate, drainPf), 1, argv);
        };
    }

    /* Subscription handler is always optional */
    if (subscriptionPf != Undefined(isolate)) {
        behavior.subscription = [subscriptionPf = std::move(subscriptionPf), isolate](auto *ws, std::string_view topic, int newCount, int oldCount) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[4] = {Local<Object>::New(isolate, perSocketData->socketPf), ArrayBuffer_New(isolate, (void *) topic.data(), topic.length()), Integer::New(isolate, newCount), Integer::New(isolate, oldCount)};
            CallJS(isolate, Local<Function>::New(isolate, subscriptionPf), 4, argv);
        };
    }

    /* Ping handler is always optional */
    if (pingPf != Undefined(isolate)) {
        behavior.ping = [pingPf = std::move(pingPf), isolate](auto *ws, std::string_view message) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[2] = {Local<Object>::New(isolate, perSocketData->socketPf), ArrayBuffer_New(isolate, (void *) message.data(), message.length())};
            CallJS(isolate, Local<Function>::New(isolate, pingPf), 2, argv);
        };
    }

    /* Pong handler is always optional */
    if (pongPf != Undefined(isolate)) {
        behavior.pong = [pongPf = std::move(pongPf), isolate](auto *ws, std::string_view message) {
            HandleScope hs(isolate);

            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            Local<Value> argv[2] = {Local<Object>::New(isolate, perSocketData->socketPf), ArrayBuffer_New(isolate, (void *) message.data(), message.length())};
            CallJS(isolate, Local<Function>::New(isolate, pongPf), 2, argv);
        };
    }

    /* Close handler is NOT optional for the wrapper */
    behavior.close = [closePf = std::move(closePf), isolate](auto *ws, int code, std::string_view message) {
        HandleScope hs(isolate);

        Local<ArrayBuffer> messageArrayBuffer = ArrayBuffer_New(isolate, (void *) message.data(), message.length());
        PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
        Local<Object> wsObject = Local<Object>::New(isolate, perSocketData->socketPf);

        /* Invalidate this wsObject */
        wsObject->SetAlignedPointerInInternalField(0, nullptr);

        /* Only call close handler if we have one set */
        Local<Function> closeLf = Local<Function>::New(isolate, closePf);
        if (!closeLf->IsUndefined()) {
            Local<Value> argv[3] = {wsObject, Integer::New(isolate, code), messageArrayBuffer};
            CallJS(isolate, closeLf, 3, argv);
        }

        /* This should technically not be required */
        perSocketData->socketPf.Reset();

        /* Again, here we clear the buffer to avoid strange bugs */
        messageArrayBuffer->Detach();
    };

    app->template ws<PerSocketData>(std::string(pattern.getString()), std::move(behavior));

    /* Return this */
    args.GetReturnValue().Set(args.Holder());
}

/* This method wraps get, post and all http methods */
template <typename APP, typename F>
void uWS_App_get(F f, const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    /* Pattern */
    NativeString pattern(args.GetIsolate(), args[0]);
    if (pattern.isInvalid(args)) {
        return;
    }

    /* If the handler is null */
    if (args[1]->IsNull()) {
        (app->*f)(std::string(pattern.getString()), nullptr);
        args.GetReturnValue().Set(args.Holder());
        return;
    }

    /* Handler */
    Callback checkedCallback(args.GetIsolate(), args[1]);
    if (checkedCallback.isInvalid(args)) {
        return;
    }
    UniquePersistent<Function> cb = checkedCallback.getFunction();

    /* This function requires perContextData */
    PerContextData *perContextData = (PerContextData *) Local<External>::Cast(args.Data())->Value();

    (app->*f)(std::string(pattern.getString()), [cb = std::move(cb), perContextData](auto *res, auto *req) {
        Isolate *isolate = perContextData->isolate;
        HandleScope hs(isolate);

        Local<Object> resObject = perContextData->resTemplate[getAppTypeIndex<APP>()].Get(isolate)->Clone();
        resObject->SetAlignedPointerInInternalField(0, res);

        Local<Object> reqObject = perContextData->reqTemplate[std::is_same<APP, uWS::H3App>::value].Get(isolate)->Clone();
        reqObject->SetAlignedPointerInInternalField(0, req);

        Local<Value> argv[] = {resObject, reqObject};
        CallJS(isolate, cb.Get(isolate), 2, argv);

        /* Properly invalidate req */
        reqObject->SetAlignedPointerInInternalField(0, nullptr);

        /* µWS itself will terminate if not responded and not attached
         * onAborted handler, so we can assume it's done */
    });

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_close(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    app->close();
    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_listen_unix(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();

    /* Require at least two arguments */
    if (missingArguments(2, args)) {
        return;
    }

    /* integer options is first (not implemented) */

    /* Callback is first */
    auto cb = [&args, isolate](auto *token) {
        /* Return a false boolean if listen failed */
        Local<Value> argv[] = {token ? Local<Value>::Cast(External::New(isolate, token)) : Local<Value>::Cast(Boolean::New(isolate, false))};
        /* Immediate call cannot be CallJS */
        Local<Function>::Cast(args[0])->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 1, argv).IsEmpty();
    };

    /* Path is last */
    std::string path;
    NativeString h(isolate, args[args.Length() - 1]);
    if (h.isInvalid(args)) {
        return;
    }
    path = h.getString();

    app->listen(std::move(cb), path);

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_listen(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();

    /* Require at least two arguments */
    if (missingArguments(2, args)) {
        return;
    }

    /* Callback is last */
    auto cb = [&args, isolate](auto *token) {
        /* Return a false boolean if listen failed */
        Local<Value> argv[] = {token ? Local<Value>::Cast(External::New(isolate, token)) : Local<Value>::Cast(Boolean::New(isolate, false))};
        /* Immediate call cannot be CallJS */
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
void uWS_App_domain(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();

    /* serverName */
    if (missingArguments(1, args)) {
        return;
    }

    NativeString serverName(isolate, args[0]);
    if (serverName.isInvalid(args)) {
        return;
    }

    app->domain(std::string(serverName.getString()));

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_publish(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();

    /* topic, message [isBinary, compress] */
    if (missingArguments(2, args)) {
        return;
    }

    NativeString topic(isolate, args[0]);
    if (topic.isInvalid(args)) {
        return;
    }

    NativeString message(isolate, args[1]);
    if (message.isInvalid(args)) {
        return;
    }

    bool ok = app->publish(topic.getString(), message.getString(), args[2]->BooleanValue(isolate) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, args[3]->BooleanValue(isolate));

    args.GetReturnValue().Set(Boolean::New(isolate, ok));
}

template <typename APP>
void uWS_App_numSubscribers(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();

    /* topic */
    if (missingArguments(1, args)) {
        return;
    }

    NativeString topic(isolate, args[0]);
    if (topic.isInvalid(args)) {
        return;
    }

    args.GetReturnValue().Set(Integer::New(isolate, app->numSubscribers(topic.getString())));
}

/* This one modified per-thread static strings temporarily */
std::pair<uWS::SocketContextOptions, bool> readOptionsObject(const FunctionCallbackInfo<Value> &args, int index) {
    Isolate *isolate = args.GetIsolate();
    /* Read the options object if any */
    uWS::SocketContextOptions options = {};
    thread_local std::string keyFileName, certFileName, passphrase, dhParamsFileName, caFileName, sslCiphers;
    if (args.Length() > index) {

        Local<Object> optionsObject = Local<Object>::Cast(args[index]);

        /* Key file name */
        NativeString keyFileNameValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "key_file_name", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (keyFileNameValue.isInvalid(args)) {
            return {};
        }
        if (keyFileNameValue.getString().length()) {
            keyFileName = keyFileNameValue.getString();
            options.key_file_name = keyFileName.c_str();
        }

        /* Cert file name */
        NativeString certFileNameValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "cert_file_name", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (certFileNameValue.isInvalid(args)) {
            return {};
        }
        if (certFileNameValue.getString().length()) {
            certFileName = certFileNameValue.getString();
            options.cert_file_name = certFileName.c_str();
        }

        /* Passphrase */
        NativeString passphraseValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "passphrase", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (passphraseValue.isInvalid(args)) {
            return {};
        }
        if (passphraseValue.getString().length()) {
            passphrase = passphraseValue.getString();
            options.passphrase = passphrase.c_str();
        }

        /* DH params file name */
        NativeString dhParamsFileNameValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "dh_params_file_name", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (dhParamsFileNameValue.isInvalid(args)) {
            return {};
        }
        if (dhParamsFileNameValue.getString().length()) {
            dhParamsFileName = dhParamsFileNameValue.getString();
            options.dh_params_file_name = dhParamsFileName.c_str();
        }

        /* CA file name */
        NativeString caFileNameValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "ca_file_name", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (caFileNameValue.isInvalid(args)) {
            return {};
        }
        if (caFileNameValue.getString().length()) {
            caFileName = caFileNameValue.getString();
            options.ca_file_name = caFileName.c_str();
        }

        /* ssl_prefer_low_memory_usage */
        options.ssl_prefer_low_memory_usage = optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "ssl_prefer_low_memory_usage", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()->BooleanValue(isolate);

        /* ssl_ciphers */
        NativeString sslCiphersValue(isolate, optionsObject->Get(isolate->GetCurrentContext(), String::NewFromUtf8(isolate, "ssl_ciphers", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
        if (sslCiphersValue.isInvalid(args)) {
            return {};
        }
        if (sslCiphersValue.getString().length()) {
            sslCiphers = sslCiphersValue.getString();
            options.ssl_ciphers = sslCiphers.c_str();
        }
    }

    return {options, true};
}

template <typename APP>
void uWS_App_addServerName(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();
    NativeString hostnamePatternValue(isolate, args[0]);
    if (hostnamePatternValue.isInvalid(args)) {
        return;
    }
    std::string hostnamePattern;
    if (hostnamePatternValue.getString().length()) {
        hostnamePattern = hostnamePatternValue.getString();
    }

    auto [options, valid] = readOptionsObject(args, 1);
    if (!valid) {
        return;
    }

    app->addServerName(hostnamePattern.c_str(), options);

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_removeServerName(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);

    Isolate *isolate = args.GetIsolate();
    NativeString hostnamePatternValue(isolate, args[0]);
    if (hostnamePatternValue.isInvalid(args)) {
        return;
    }
    std::string hostnamePattern;
    if (hostnamePatternValue.getString().length()) {
        hostnamePattern = hostnamePatternValue.getString();
    }

    app->removeServerName(hostnamePattern.c_str());

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App_missingServerName(const FunctionCallbackInfo<Value> &args) {
    APP *app = (APP *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();

    UniquePersistent<Function> missingPf;
    missingPf.Reset(args.GetIsolate(), Local<Function>::Cast(args[0]));

    app->missingServerName([missingPf = std::move(missingPf), isolate](const char *hostname) {
        /* We hand a JavaScript string here */
        HandleScope hs(isolate);
        Local<Function> missingLf = Local<Function>::New(isolate, missingPf);
        Local<Value> argv[1] = {String::NewFromUtf8(isolate, hostname, NewStringType::kNormal).ToLocalChecked()};
        CallJS(isolate, missingLf, 1, argv);
    });

    args.GetReturnValue().Set(args.Holder());
}

template <typename APP>
void uWS_App(const FunctionCallbackInfo<Value> &args) {

    Isolate *isolate = args.GetIsolate();
    Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);
    appTemplate->SetClassName(String::NewFromUtf8(isolate, std::is_same<APP, uWS::SSLApp>::value ? "uWS.SSLApp" : "uWS.App", NewStringType::kNormal).ToLocalChecked());

    auto [options, valid] = readOptionsObject(args, 0);
    if (!valid) {
        return;
    }

    APP *app;

    if constexpr (!std::is_same<APP, uWS::H3App>::value) {

        /* uSockets copies strings here */
        app = new APP(options);

        /* Throw if we failed to construct the app */
        if (app->constructorFailed()) {
            delete app;
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "App construction failed", NewStringType::kNormal).ToLocalChecked())));
            return;
        }

    } else {

        appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.H3App", NewStringType::kNormal).ToLocalChecked());

        app = new APP(options);
    }

    appTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    /* All the http methods */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "get", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::get, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "post", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::post, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "options", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::options, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "del", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::del, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "patch", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::patch, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "put", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::put, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "head", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::head, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "connect", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::connect, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "trace", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::trace, args);
    }, args.Data()));

    /* Any http method */
    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "any", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, [](auto &args) {
        uWS_App_get<APP>(&APP::any, args);
    }, args.Data()));

    appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_listen<APP>, args.Data()));
    
    if constexpr (!std::is_same<APP, uWS::H3App>::value) {

        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_close<APP>, args.Data()));
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen_unix", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_listen_unix<APP>, args.Data()));

        /* ws, listen */
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "ws", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_ws<APP>, args.Data()));
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "publish", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_publish<APP>, args.Data()));
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "numSubscribers", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_numSubscribers<APP>, args.Data()));

        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "domain", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_domain<APP>, args.Data()));

        /* SNI */
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "addServerName", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_addServerName<APP>, args.Data()));
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "removeServerName", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_removeServerName<APP>, args.Data()));
        appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "missingServerName", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_App_missingServerName<APP>, args.Data()));

    }

    Local<Object> localApp = appTemplate->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    localApp->SetAlignedPointerInInternalField(0, app);

    PerContextData *perContextData = (PerContextData *) Local<External>::Cast(args.Data())->Value();


    if constexpr (!std::is_same<APP, uWS::H3App>::value) {

        /* Add this to our delete list */
        if constexpr (std::is_same<APP, uWS::SSLApp>::value) {
            perContextData->sslApps.emplace_back(app);
        } else {
            perContextData->apps.emplace_back(app);
        }

    }

    args.GetReturnValue().Set(localApp);

}