// ws.getBufferedAmount()
// ws.close(lalala)
// ws.?

/* Helping QtCreator */
#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

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
