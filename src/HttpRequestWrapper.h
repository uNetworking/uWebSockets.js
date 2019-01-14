#include "App.h"
#include <v8.h>
#include "Utilities.h"
using namespace v8;

/* This one is the same for SSL and non-SSL */
struct HttpRequestWrapper {
    static Persistent<Object> reqTemplate;

    // todo: refuse all function calls if we are not inside correct callback

    static inline uWS::HttpRequest *getHttpRequest(const FunctionCallbackInfo<Value> &args) {
        return ((uWS::HttpRequest *) args.Holder()->GetAlignedPointerFromInternalField(0));
    }

    // req.onAbort ?

    /* Takes int, returns string (must be in bounds) */
    static void req_getParameter(const FunctionCallbackInfo<Value> &args) {
        int index = args[0]->Uint32Value();
        std::string_view parameter = getHttpRequest(args)->getParameter(index);

        args.GetReturnValue().Set(String::NewFromUtf8(isolate, parameter.data(), v8::String::kNormalString, parameter.length()));
    }

    /* Takes nothing, returns string */
    static void req_getUrl(const FunctionCallbackInfo<Value> &args) {
        std::string_view url = getHttpRequest(args)->getUrl();

        args.GetReturnValue().Set(String::NewFromUtf8(isolate, url.data(), v8::String::kNormalString, url.length()));
    }

    /* Takes String, returns String */
    static void req_getHeader(const FunctionCallbackInfo<Value> &args) {
        NativeString data(args.GetIsolate(), args[0]);
        char *buf = data.getData(); int length = data.getLength();

        std::string_view header = getHttpRequest(args)->getHeader(std::string_view(buf, length));

        args.GetReturnValue().Set(String::NewFromUtf8(isolate, header.data(), v8::String::kNormalString, header.length()));
    }

    static void initReqTemplate() {
        /* The only thing this req needs is getHeader and similar, getParameter, getUrl and so on */

        /*reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
        reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);*/

        /* We do clone every request object, we could share them, they are illegal to use outside the function anyways */
        Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
        reqTemplateLocal->SetClassName(String::NewFromUtf8(isolate, "uWS.HttpRequest"));
        reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);

        /* Register our functions */
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, req_getHeader));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getParameter"), FunctionTemplate::New(isolate, req_getParameter));
        reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getUrl"), FunctionTemplate::New(isolate, req_getUrl));

        /* Create the template */
        Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
        reqTemplate.Reset(isolate, reqObjectLocal);
    }

    //template <class APP>
    static Local<Object> getReqInstance() {
        return Local<Object>::New(isolate, reqTemplate)->Clone();
    }
};

Persistent<Object> HttpRequestWrapper::reqTemplate;
