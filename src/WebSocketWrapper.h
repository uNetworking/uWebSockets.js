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

#include "Utilities.h"

#include "v8-fast-api-calls.h"

/* todo: probably isCorked, cork should be exposed? */

namespace WebSocketWrapper {

    template <bool SSL>
    inline uWS::WebSocket<SSL, true, PerSocketData> *getWebSocket(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = (uWS::WebSocket<SSL, true, PerSocketData> *) args.This()->GetAlignedPointerFromInternalField(0);
        if (!ws) {
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "Invalid access of closed uWS.WebSocket/SSLWebSocket.", NewStringType::kNormal).ToLocalChecked())));
        }
        return ws;
    }

    inline void invalidateWsObject(args_t args) {
        args.This()->SetAlignedPointerInInternalField(0, nullptr);
    }

    /* Takes nothing returns holder (only used to fool TypeScript, as a conversion from WS to UserData) */
    void uWS_WebSocket_getUserData(args_t args) {
        args.GetReturnValue().Set(args.This());
    }

    /* Takes string topic */
    template <bool SSL>
    void uWS_WebSocket_subscribe(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString<true> topic(isolate, args[0]);
            if (topic.isInvalid(args)) {
                return;
            }
            bool success = ws->subscribe(topic.getString());
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* Takes string topic, returns boolean success */
    template <bool SSL>
    void uWS_WebSocket_unsubscribe(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString<true> topic(isolate, args[0]);
            if (topic.isInvalid(args)) {
                return;
            }
            bool success = ws->unsubscribe(topic.getString());
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* Takes string topic, message, returns boolean success */
    template <bool SSL>
    void uWS_WebSocket_publish(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            if (missingArguments(2, args)) {
                return;
            }

            bool isBinary = args[2]->BooleanValue(isolate);
            bool compress = args[3]->BooleanValue(isolate);

            NativeString<true> topic(isolate, args[0]);
            if (topic.isInvalid(args)) {
                return;
            }
            NativeString<true> message(isolate, args[1]);
            if (message.isInvalid(args)) {
                return;
            }

            bool success = ws->publish(topic.getString(), message.getString(), isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, compress);
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* It would make sense to call terminate "close" and call close "end" to line up with HTTP */
    /* That also makes sense seince close takes message and code -> you can end with a string message */

    /* Takes nothing returns nothing */
    template <bool SSL>
    void uWS_WebSocket_close(args_t args) {
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            invalidateWsObject(args);
            ws->close();
        }
    }

    /* Takes code, message, returns undefined */
    template <bool SSL>
    void uWS_WebSocket_end(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            int code = 0;
            if (args.Length() >= 1) {
                code = args[0]->Uint32Value(isolate->GetCurrentContext()).ToChecked();
            }

            NativeString<true> message(args.GetIsolate(), args[1]);
            if (message.isInvalid(args)) {
                return;
            }

            invalidateWsObject(args);
            ws->end(code, message.getString());
        }
    }

    /* Takes nothing returns arraybuffer */
    template <bool SSL>
    void uWS_WebSocket_getRemoteAddress(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            std::string_view ip = ws->getRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing returns arraybuffer */
    template <bool SSL>
    void uWS_WebSocket_getRemoteAddressAsText(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            std::string_view ip = ws->getRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    void uWS_WebSocket_getRemotePort(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            unsigned int port = ws->getRemotePort();
            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    void uWS_WebSocket_getBufferedAmount(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            unsigned int bufferedAmount = ws->getBufferedAmount();
            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, bufferedAmount));
        }
    }

    /* Takes message, isBinary, compressed. Returns true on success, false otherwise */
    template <bool SSL>
    void uWS_WebSocket_sendFirstFragment(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            unsigned int sendStatus = ws->sendFirstFragment(message.getString(), args[1]->BooleanValue(isolate) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, args[2]->BooleanValue(isolate));

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes message, compressed. Returns true on success, false otherwise */
    template <bool SSL>
    void uWS_WebSocket_sendFragment(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            unsigned int sendStatus = ws->sendFragment(message.getString(), args[1]->BooleanValue(isolate));

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes message, compressed. Returns true on success, false otherwise */
    template <bool SSL>
    void uWS_WebSocket_sendLastFragment(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            unsigned int sendStatus = ws->sendLastFragment(message.getString(), args[1]->BooleanValue(isolate));

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes message, isBinary, compressed. Returns true on success, false otherwise */
    template <bool SSL>
    void uWS_WebSocket_send(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {

            bool isBinary = args[1]->BooleanValue(isolate);
            bool compress = args[2]->BooleanValue(isolate);

            NativeString<true> message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            unsigned int sendStatus = ws->send(message.getString(), isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, compress);

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes topic string, returns bool */
    template <bool SSL>
    void uWS_WebSocket_isSubscribed(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString<true> topic(args.GetIsolate(), args[0]);
            if (topic.isInvalid(args)) {
                return;
            }

            bool subscribed = ws->isSubscribed(topic.getString());

            args.GetReturnValue().Set(Boolean::New(isolate, subscribed));
        }
    }

    /* Takes message. Returns true on success, false otherwise */
    template <bool SSL>
    void uWS_WebSocket_ping(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString<true> message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            /* This is a wrapper that does not exist in the C++ project */
            unsigned int sendStatus = ws->send(message.getString(), uWS::OpCode::PING);

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes function, returns this */
    template <bool SSL>
    void uWS_WebSocket_cork(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {

            ws->cork([cb = Local<Function>::Cast(args[0]), isolate]() {
                /* No need for CallJS here */
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 0, nullptr).IsEmpty();
            });

            args.GetReturnValue().Set(args.This());
        }
    }

    /* This one is wrapped instead of iterateTopics as JS-people will put their hands in wood chipper for sure. */
    template <bool SSL>
    void uWS_WebSocket_getTopics(args_t args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {

            Local<Array> topicsArray = Array::New(isolate, 0);

            ws->iterateTopics([&topicsArray, isolate](std::string_view topic) {
                Local<String> topicString = String::NewFromUtf8(isolate, topic.data(), NewStringType::kNormal, topic.length()).ToLocalChecked();

                topicsArray->Set(isolate->GetCurrentContext(), topicsArray->Length(), topicString).IsNothing();
            });

            args.GetReturnValue().Set(topicsArray);
        }
    }

    /* V8 fast call fast path for send - called directly from JIT-optimised code.
     * Requirements: no JS heap allocation, no JS execution, Uint8Array arg only.
     * A null internal-field pointer (closed socket) sets options.fallback = true so V8
     * re-invokes the slow path which throws the proper exception. */

// Version A: Handles Strings
template <bool SSL>
uint32_t uWS_WebSocket_send_fast_string(v8::Local<v8::Object> receiver, 
                                               const v8::FastOneByteString& message, 
                                               bool isBinary, bool compress) {
    auto *ws = (uWS::WebSocket<SSL, true, PerSocketData> *) receiver->GetAlignedPointerFromInternalField(0);
    if (!ws) return 0;
    return ws->send(std::string_view(message.data, message.length),
                    isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, compress);
}

// Version B: Handles ArrayBuffer/TypedArray
template <bool SSL>
uint32_t uWS_WebSocket_send_fast_buffer(v8::Local<v8::Object> receiver, 
                                               v8::Local<v8::Value> message, 
                                               bool isBinary, bool compress) {
    auto *ws = (uWS::WebSocket<SSL, true, PerSocketData> *) receiver->GetAlignedPointerFromInternalField(0);
    if (!ws) return 0;

    char* data = nullptr;
    size_t length = 0;

    if (message->IsArrayBufferView()) {
        auto view = message.As<v8::ArrayBufferView>();
        length = view->ByteLength();
        data = static_cast<char*>(view->Buffer()->GetBackingStore()->Data()) + view->ByteOffset();
    } else if (message->IsArrayBuffer()) {
        auto ab = message.As<v8::ArrayBuffer>();
        length = ab->ByteLength();
        data = static_cast<char*>(ab->GetBackingStore()->Data());
    } else {
        // Not a buffer type we handle fast
        return 0; 
    }
    
    return ws->send(std::string_view(data, length),
                    isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, compress);
}

    template <bool SSLOption>
    Local<Object> init(Isolate *isolate) {
        Local<FunctionTemplate> wsTemplateLocal = FunctionTemplate::New(isolate);
        
        wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, SSLOption == 1 ? "uWS.SSLWebSocket" : "uWS.WebSocket", NewStringType::kNormal).ToLocalChecked());

        wsTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* helper */
        auto regFn = [wsObjectTemplate = wsTemplateLocal->PrototypeTemplate(), isolate]<size_t N>(
          const char (&str)[N],
          void(*cb)(args_t)
        ){
          wsObjectTemplate->Set(
            String::NewFromUtf8(isolate, str, NewStringType::kNormal, N-1).ToLocalChecked(),
            FunctionTemplate::New(isolate, cb)
          );
        };

        /* Register our functions */
        regFn("sendFirstFragment", uWS_WebSocket_sendFirstFragment<SSLOption>);
        regFn("sendFragment", uWS_WebSocket_sendFragment<SSLOption>);
        regFn("sendLastFragment", uWS_WebSocket_sendLastFragment<SSLOption>);

        regFn("getUserData", uWS_WebSocket_getUserData);

        static v8::CFunction fast_send = v8::CFunction::Make(uWS_WebSocket_send_fast_buffer<SSLOption>);

        regFn("send", uWS_WebSocket_send<SSLOption>/*, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow, SideEffectType::kHasSideEffect, &fast_send*/);
        regFn("end", uWS_WebSocket_end<SSLOption>);
        regFn("close", uWS_WebSocket_close<SSLOption>);
        regFn("getBufferedAmount", uWS_WebSocket_getBufferedAmount<SSLOption>);
        regFn("getRemoteAddress", uWS_WebSocket_getRemoteAddress<SSLOption>);
        regFn("subscribe", uWS_WebSocket_subscribe<SSLOption>);
        regFn("unsubscribe", uWS_WebSocket_unsubscribe<SSLOption>);
        regFn("publish", uWS_WebSocket_publish<SSLOption>);
        regFn("cork", uWS_WebSocket_cork<SSLOption>);
        regFn("ping", uWS_WebSocket_ping<SSLOption>);
        regFn("getRemoteAddressAsText", uWS_WebSocket_getRemoteAddressAsText<SSLOption>);
        regFn("getRemotePort", uWS_WebSocket_getRemotePort<SSLOption>);
        regFn("isSubscribed", uWS_WebSocket_isSubscribed<SSLOption>);

        /* This one does not exist in C++ */
        regFn("getTopics", uWS_WebSocket_getTopics<SSLOption>);

        /* Create the template */
        Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return wsObjectLocal;
    }
};
