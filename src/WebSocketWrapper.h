/*
 * Authored by Alex Hultman, 2018-2020.
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
#include "Utilities.h"

#include <v8.h>
using namespace v8;

/* todo: probably isCorked, cork should be exposed? */

struct WebSocketWrapper {

    template <bool SSL>
    static inline uWS::WebSocket<SSL, true, PerSocketData> *getWebSocket(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = (uWS::WebSocket<SSL, true, PerSocketData> *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!ws) {
            args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(isolate, "Invalid access of closed uWS.WebSocket/SSLWebSocket.", NewStringType::kNormal).ToLocalChecked())));
        }
        return ws;
    }

    static inline void invalidateWsObject(const FunctionCallbackInfo<Value> &args) {
        args.Holder()->SetAlignedPointerInInternalField(0, nullptr);
    }

    /* Takes nothing returns holder (only used to fool TypeScript, as a conversion from WS to UserData) */
    template <bool SSL>
    static void uWS_WebSocket_getUserData(const FunctionCallbackInfo<Value> &args) {
        args.GetReturnValue().Set(args.Holder());
    }

    /* Takes string topic */
    template <bool SSL>
    static void uWS_WebSocket_subscribe(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString topic(isolate, args[0]);
            if (topic.isInvalid(args)) {
                return;
            }
            bool nonStrict = args.Length() > 1 && args[1]->BooleanValue(isolate);
            bool success = ws->subscribe(topic.getString(), nonStrict);
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* Takes string topic, returns boolean success */
    template <bool SSL>
    static void uWS_WebSocket_unsubscribe(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString topic(isolate, args[0]);
            if (topic.isInvalid(args)) {
                return;
            }
            bool nonStrict = args.Length() > 1 && args[1]->BooleanValue(isolate);
            bool success = ws->unsubscribe(topic.getString(), nonStrict);
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* Takes string topic, message, returns boolean success */
    template <bool SSL>
    static void uWS_WebSocket_publish(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
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

            bool success = ws->publish(topic.getString(), message.getString(), args[2]->BooleanValue(isolate) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, args[3]->BooleanValue(isolate));
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* It would make sense to call terminate "close" and call close "end" to line up with HTTP */
    /* That also makes sense seince close takes message and code -> you can end with a string message */

    /* Takes nothing returns nothing */
    template <bool SSL>
    static void uWS_WebSocket_close(const FunctionCallbackInfo<Value> &args) {
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            invalidateWsObject(args);
            ws->close();
        }
    }

    /* Takes code, message, returns undefined */
    template <bool SSL>
    static void uWS_WebSocket_end(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            int code = 0;
            if (args.Length() >= 1) {
                code = args[0]->Uint32Value(isolate->GetCurrentContext()).ToChecked();
            }

            NativeString message(args.GetIsolate(), args[1]);
            if (message.isInvalid(args)) {
                return;
            }

            invalidateWsObject(args);
            ws->end(code, message.getString());
        }
    }

    /* Takes nothing returns arraybuffer */
    template <bool SSL>
    static void uWS_WebSocket_getRemoteAddress(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            std::string_view ip = ws->getRemoteAddress();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing returns arraybuffer */
    template <bool SSL>
    static void uWS_WebSocket_getRemoteAddressAsText(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            std::string_view ip = ws->getRemoteAddressAsText();

            args.GetReturnValue().Set(ArrayBuffer_NewCopy(isolate, (void *) ip.data(), ip.length()));
        }
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    static void uWS_WebSocket_getRemotePort(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            unsigned int port = ws->getRemotePort();
            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, port));
        }
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    static void uWS_WebSocket_getBufferedAmount(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            unsigned int bufferedAmount = ws->getBufferedAmount();
            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, bufferedAmount));
        }
    }

    /* Takes message, isBinary, compressed. Returns true on success, false otherwise */
    template <bool SSL>
    static void uWS_WebSocket_sendFirstFragment(const FunctionCallbackInfo<Value> &args) {
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
    static void uWS_WebSocket_sendFragment(const FunctionCallbackInfo<Value> &args) {
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
    static void uWS_WebSocket_sendLastFragment(const FunctionCallbackInfo<Value> &args) {
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
    static void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            unsigned int sendStatus = ws->send(message.getString(), args[1]->BooleanValue(isolate) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, args[2]->BooleanValue(isolate));

            args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, sendStatus));
        }
    }

    /* Takes topic string, returns bool */
    template <bool SSL>
    static void uWS_WebSocket_isSubscribed(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString topic(args.GetIsolate(), args[0]);
            if (topic.isInvalid(args)) {
                return;
            }

            bool subscribed = ws->isSubscribed(topic.getString());

            args.GetReturnValue().Set(Boolean::New(isolate, subscribed));
        }
    }

    /* Takes message. Returns true on success, false otherwise */
    template <bool SSL>
    static void uWS_WebSocket_ping(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
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
    static void uWS_WebSocket_cork(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {

            ws->cork([cb = Local<Function>::Cast(args[0]), isolate]() {
                /* No need for CallJS here */
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 0, nullptr).IsEmpty();
            });

            args.GetReturnValue().Set(args.Holder());
        }
    }

    /* This one is wrapped instead of iterateTopics as JS-people will put their hands in wood chipper for sure. */
    template <bool SSL>
    static void uWS_WebSocket_getTopics(const FunctionCallbackInfo<Value> &args) {
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

    template <bool SSL>
    static Local<Object> init(Isolate *isolate) {
        Local<FunctionTemplate> wsTemplateLocal = FunctionTemplate::New(isolate);
        if (SSL) {
            wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLWebSocket", NewStringType::kNormal).ToLocalChecked());
        } else {
            wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.WebSocket", NewStringType::kNormal).ToLocalChecked());
        }
        wsTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "sendFirstFragment", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_sendFirstFragment<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "sendFragment", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_sendFragment<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "sendLastFragment", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_sendLastFragment<SSL>));

        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getUserData", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getUserData<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_send<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_end<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_close<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getBufferedAmount", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getBufferedAmount<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getRemoteAddress<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "subscribe", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_subscribe<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "unsubscribe", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_unsubscribe<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "publish", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_publish<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "cork", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_cork<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "ping", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_ping<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddressAsText", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getRemoteAddressAsText<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemotePort", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getRemotePort<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "isSubscribed", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_isSubscribed<SSL>));

        /* This one does not exist in C++ */
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getTopics", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getTopics<SSL>));

        /* Create the template */
        Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

        return wsObjectLocal;
    }
};
