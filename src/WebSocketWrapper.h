#include "App.h"
#include "Utilities.h"

#include <v8.h>
using namespace v8;

/* todo: probably isCorked, cork should be exposed? */

struct WebSocketWrapper {

    template <bool SSL>
    static inline uWS::WebSocket<SSL, true> *getWebSocket(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = (uWS::WebSocket<SSL, true> *) args.Holder()->GetAlignedPointerFromInternalField(0);
        if (!ws) {
            args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, "Invalid access of closed uWS.WebSocket/SSLWebSocket.", NewStringType::kNormal).ToLocalChecked()));
        }
        return ws;
    }

    static inline void invalidateWsObject(const FunctionCallbackInfo<Value> &args) {
        args.Holder()->SetAlignedPointerInInternalField(0, nullptr);
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
            ws->subscribe(topic.getString());
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
            bool success = ws->unsubscribe(topic.getString());
            args.GetReturnValue().Set(Boolean::New(isolate, success));
        }
    }

    /* Takes string topic, message */
    template <bool SSL>
    static void uWS_WebSocket_publish(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString topic(isolate, args[0]);
            NativeString message(isolate, args[1]);
            ws->publish(topic.getString(), message.getString(), BooleanValue(isolate, args[2]) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, BooleanValue(isolate, args[3]));
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

            /* Todo: we need to pass a copy here */
            args.GetReturnValue().Set(ArrayBuffer::New(isolate, (void *) ip.data(), ip.length()/*, ArrayBufferCreationMode::kInternalized*/));
        }
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    static void uWS_WebSocket_getBufferedAmount(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            int bufferedAmount = ws->getBufferedAmount();
            args.GetReturnValue().Set(Integer::New(isolate, bufferedAmount));
        }
    }

    /* Takes message, isBinary. Returns true on success, false otherwise */
    template <bool SSL>
    static void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            NativeString message(args.GetIsolate(), args[0]);
            if (message.isInvalid(args)) {
                return;
            }

            bool ok = ws->send(message.getString(), BooleanValue(isolate, args[1]) ? uWS::OpCode::BINARY : uWS::OpCode::TEXT, BooleanValue(isolate, args[2]));

            args.GetReturnValue().Set(Boolean::New(isolate, ok));
        }
    }

    /* Takes nothing, returns this */
    /*template <bool SSL>
    static void uWS_WebSocket_unsubscribeAll(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {
            ws->unsubscribeAll();
            args.GetReturnValue().Set(args.Holder());
        }
    }*/

    /* Takes function, returns this */
    template <bool SSL>
    static void uWS_WebSocket_cork(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        auto *ws = getWebSocket<SSL>(args);
        if (ws) {

            ws->cork([cb = Local<Function>::Cast(args[0]), isolate]() {
                cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 0, nullptr).IsEmpty();
            });

            args.GetReturnValue().Set(args.Holder());
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
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_send<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_end<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_close<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getBufferedAmount", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getBufferedAmount<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getRemoteAddress", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_getRemoteAddress<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "subscribe", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_subscribe<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "unsubscribe", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_unsubscribe<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "publish", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_publish<SSL>));
        //wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "unsubscribeAll", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_unsubscribeAll<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "cork", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, uWS_WebSocket_cork<SSL>));

        /* Create the template */
        Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        
        return wsObjectLocal;
    }
};
