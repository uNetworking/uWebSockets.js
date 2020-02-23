#ifndef ADDON_UTILITIES_H
#define ADDON_UTILITIES_H

#include <v8.h>
using namespace v8;

/* Unfortunately we have to depend on Node.js garbage */
#include <node.h>

/* This is a very hot function ruined by illiteracy */
MaybeLocal<Value> CallJS(Isolate *isolate, Local<Function> f, int argc, Local<Value> *argv) {
    if (!experimental_fastcall) {
        /* Node.js is built by incompetent people who should never have touched a computer in the first place */
        return node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), f, argc, argv, {0, 0});
    } else {
        /* Google LLC don't hire incompetent people to work on their stuff */
        return f->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), argc, argv);
    }
}

struct PerContextData {
    Isolate *isolate;
    UniquePersistent<Object> reqTemplate;
    UniquePersistent<Object> resTemplate[2];
    UniquePersistent<Object> wsTemplate[2];

    /* We hold all apps until free */
    std::vector<std::unique_ptr<uWS::App>> apps;
    std::vector<std::unique_ptr<uWS::SSLApp>> sslApps;
};

template <class APP>
static constexpr int getAppTypeIndex() {
    /* Returns 1 for SSLApp and 0 for App */
    return std::is_same<APP, uWS::SSLApp>::value;
}

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
    bool invalid = false;
public:
    NativeString(Isolate *isolate, const Local<Value> &value) {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            utf8Value = new (utf8ValueMemory) String::Utf8Value(isolate, value);
            data = (**utf8Value);
            length = utf8Value->length();
        } else if (value->IsTypedArray()) {
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            ArrayBuffer::Contents contents = arrayBufferView->Buffer()->GetContents();
            length = arrayBufferView->ByteLength();
            data = (char *) contents.Data() + arrayBufferView->ByteOffset();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            ArrayBuffer::Contents contents = arrayBuffer->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else {
            invalid = true;
        }
    }

    bool isInvalid(const FunctionCallbackInfo<Value> &args) {
        if (invalid) {
            args.GetReturnValue().Set(args.GetIsolate()->ThrowException(String::NewFromUtf8(args.GetIsolate(), "Text and data can only be passed by String, ArrayBuffer or TypedArray.", NewStringType::kNormal).ToLocalChecked()));
        }
        return invalid;
    }

    std::string_view getString() {
        return {data, length};
    }

    ~NativeString() {
        if (utf8Value) {
            utf8Value->~Utf8Value();
        }
    }
};

#endif
