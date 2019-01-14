#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

struct WebSocketWrapper {
    static Persistent<Object> wsTemplate[2];

    // ws.getBufferedAmount()
    // ws.close(lalala)
    // ws.?

    template <bool SSL>
    static void uWS_WebSocket_send(const FunctionCallbackInfo<Value> &args) {
        NativeString nativeString(args.GetIsolate(), args[0]);

        bool isBinary = args[1]->Int32Value();

        bool ok = ((uWS::WebSocket<SSL, true> *) args.Holder()->GetAlignedPointerFromInternalField(0))->send(
                    std::string_view(nativeString.getData(), nativeString.getLength()), isBinary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT
                    );

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
        wsTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "send"), FunctionTemplate::New(isolate, uWS_WebSocket_send<SSL>));

        Local<Object> wsObjectLocal = wsTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        wsTemplate[SSL].Reset(isolate, wsObjectLocal);
    }

    template <class APP>
    static Local<Object> getWsInstance() {
        return Local<Object>::New(isolate, wsTemplate[std::is_same<APP, uWS::SSLApp>::value])->Clone();
    }
};

Persistent<Object> WebSocketWrapper::wsTemplate[2];
