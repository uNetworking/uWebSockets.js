#include <uv.h>
#include <node.h>
#include <node_buffer.h>
#include <iostream>
using namespace v8;

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
public:
    NativeString(const Local<Value> &value) {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            utf8Value = new (utf8ValueMemory) String::Utf8Value(value);
            data = (**utf8Value);
            length = utf8Value->length();
        } else if (node::Buffer::HasInstance(value)) {
            data = node::Buffer::Data(value);
            length = node::Buffer::Length(value);
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

#include "App.h"
Isolate *isolate;
Persistent<Object> resTemplate;
Persistent<Object> reqTemplate;

void res_end(const FunctionCallbackInfo<Value> &args) {

	// you might want to do extra work here to swap to tryEnd if passed a Buffer?
	// or always use tryEnd and simply grab the object as persistent?

	NativeString data(args[0]);
        ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->end(std::string_view(data.getData(), data.getLength()));

	// Return this
	args.GetReturnValue().Set(args.Holder());
}

void res_writeHeader(const FunctionCallbackInfo<Value> &args) {
	// get string
	NativeString header(args[0]);
	NativeString value(args[1]);

        ((uWS::HttpResponse<false> *) args.Holder()->GetAlignedPointerFromInternalField(0))->writeHeader(std::string_view(header.getData(), header.getLength()), std::string_view(value.getData(), value.getLength()));

	args.GetReturnValue().Set(args.Holder());
}

void req_getHeader(const FunctionCallbackInfo<Value> &args) {
	// get string
	NativeString data(args[0]);
	char *buf = data.getData(); int length = data.getLength();

        std::string_view header = ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0))->getHeader(std::string_view(buf, length));

	args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
}

void uWS_App_get(const FunctionCallbackInfo<Value> &args) {
	uWS::App *app = (uWS::App *) args.Holder()->GetAlignedPointerFromInternalField(0);

	NativeString nativeString(args[0]);

	Persistent<Function> *pf = new Persistent<Function>();
	pf->Reset(args.GetIsolate(), Local<Function>::Cast(args[1]));

	app->get(std::string(nativeString.getData(), nativeString.getLength()), [pf](auto *res, auto *req) {
		HandleScope hs(isolate);

		Local<Object> resObject = Local<Object>::New(isolate, resTemplate)->Clone();
		resObject->SetAlignedPointerInInternalField(0, res);

		Local<Object> reqObject = Local<Object>::New(isolate, reqTemplate)->Clone();
		reqObject->SetAlignedPointerInInternalField(0, req);

		// node:: is horrible but 
		Local<Value> argv[] = {resObject, reqObject};
		node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *pf), 2, argv);

	});

	args.GetReturnValue().Set(args.Holder());
}

// port, callback?
void uWS_App_listen(const FunctionCallbackInfo<Value> &args) {
	uWS::App *app = (uWS::App *) args.Holder()->GetAlignedPointerFromInternalField(0);

	int port = args[0]->Uint32Value();

	app->listen(port, [&args](auto *token) {
		Local<Value> argv[] = {Boolean::New(isolate, token)};
		Local<Function>::Cast(args[1])->Call(isolate->GetCurrentContext()->Global(), 1, argv);
	});

	// Return this
	args.GetReturnValue().Set(args.Holder());
}

// uWS.App() -> object
void uWS_App(const FunctionCallbackInfo<Value> &args) {
	// uWS.App
	Local<FunctionTemplate> appTemplate = FunctionTemplate::New(isolate);
	appTemplate->SetClassName(String::NewFromUtf8(isolate, "uWS.App"));
	appTemplate->InstanceTemplate()->SetInternalFieldCount(1);
 
	// Get
	appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "get"), FunctionTemplate::New(isolate, uWS_App_get));

	// Listen
	appTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen"), FunctionTemplate::New(isolate, uWS_App_listen));

	// Instantiate and set intenal pointer
	Local<Object> localApp = appTemplate->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

	// Delete this boy
	localApp->SetAlignedPointerInInternalField(0, new uWS::App());

	// Return an instance of this shit
	args.GetReturnValue().Set(localApp);
}

void Main(Local<Object> exports) {
	isolate = exports->GetIsolate();

	/*reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
	reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);*/

	// App is a function returning an object
	NODE_SET_METHOD(exports, "App", uWS_App);

	// HttpResponse template
	Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
	resTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpResponse"));
	resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
	resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, res_end));
	resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHeader"), FunctionTemplate::New(isolate, res_writeHeader));

	Local<Object> resObjectLocal = resTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
	resTemplate.Reset(isolate, resObjectLocal);

	// Request template
	Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
	reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
	reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
	reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));

	Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
	reqTemplate.Reset(isolate, reqObjectLocal);
}

NODE_MODULE(uWS, Main)
