#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

// todo: also check for use after free here!
// todo: probably isCorked, cork should be exposed?

struct WebSocketWrapper {
    static Persistent<Object> wsTemplate[2];

    template <bool SSL>
    static inline uWS::WebSocket<SSL, true> *getWebSocket(const FunctionCallbackInfo<Value> &args) {
        return ((uWS::WebSocket<SSL, true> *) args.Holder()->GetAlignedPointerFromInternalField(0));
    }

    /* Takes code, message, returns undefined */
    template <bool SSL>
    static void uWS_WebSocket_close(const FunctionCallbackInfo<Value> &args) {
        int code = 0;
        if (args.Length() >= 1) {
            code = args[0]->Uint32Value();
        }

        NativeString message(args.GetIsolate(), args[1]);
        if (message.isInvalid(args)) {
            return;
        }

        getWebSocket<SSL>(args)->close(code, message.getString());
    }

    /* Takes nothing, returns integer */
    template <bool SSL>
    static void uWS_WebSocket_getBufferedAmount(const FunctionCallbackInfo<Value> &args) {
        int bufferedAmount = getWebSocket<SSL>(args)->getBufferedAmount();
        args.GetReturnValue().Set(Integer::New(isolate, bufferedAmount));
    }

    /* Takes message, isBinary. Returns true on success, false otherwise */
    template <bool SSL>
    static void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
        NativeString message(args.GetIsolate(), args[0]);
        if (message.isInvalid(args)) {
            return;
        }

        bool isBinary = args[1]->BooleanValue();

        bool ok = getWebSocket<SSL>(args)->send(message.getString(), isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT);

        args.GetReturnValue().Set(Boolean::New(isolate, ok));
    }

    template <bool SSL>
    static void initWsTemplate() {
        Local<FunctionTemplate> wsTemplateLocal = FunctionTemplate::New(isolate);
        if (SSL) {
            wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.SSLWebSocket"));
        } else {
            wsTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.WebSocket"));
        }
        wsTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send"), FunctionTemplate::New(isolate, uWS_WebSocket_send<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "close"), FunctionTemplate::New(isolate, uWS_WebSocket_close<SSL>));
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getBufferedAmount"), FunctionTemplate::New(isolate, uWS_WebSocket_getBufferedAmount<SSL>));

        /* Create the template */
        Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        wsTemplate[SSL].Reset(isolate, wsObjectLocal);
    }

    /* This is where we output an instance */
    template <class APP>
    static Local<Object> getWsInstance() {
        return Local<Object>::New(isolate, wsTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
    }
};

/* Fix this, should be nicer */
Persistent<Object> WebSocketWrapper::wsTemplate[2];
