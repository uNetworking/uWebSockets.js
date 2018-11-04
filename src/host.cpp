/* Welcome to the loopery host sources */
#include <libplatform/libplatform.h>
#include <v8.h>
using namespace v8;

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#include "App.h"

// We load features from the addon
extern void Main(Local<Object> exports);

// console.log
void print(const FunctionCallbackInfo<Value> &args) {
    String::Utf8Value utf8(args.GetIsolate(), args[0]);
    std::cout << *utf8 << std::endl;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <javascript source file>" << std::endl;
        return 0;
    }

    // load script sources
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string code = buffer.str();

    // inte den blekaste
    V8::InitializeICUDefaultLocation(argv[0]);
    V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
    V8::InitializePlatform(platform.get());
    V8::Initialize();

    // isolate vet vi typ vad det är
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
    Isolate *isolate = Isolate::New(create_params);
    {
        Isolate::Scope isolate_scope(isolate);
        HandleScope handle_scope(isolate);

        // register functions (here is where addon.cpp comes in)
        Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

        // at least register print to global
        global->Set(String::NewFromUtf8(isolate, "print", NewStringType::kNormal).ToLocalChecked(),
        FunctionTemplate::New(isolate, print));

        // vi skapar ett context som håller det globala objektet
        Local<Context> context = Context::New(isolate, nullptr, global);
        Context::Scope context_scope(context);

        // register µWS features
        Main(context->Global());

        // ladda in scriptet (preprocessa include)
        Local<String> source = String::NewFromUtf8(isolate, code.data(), NewStringType::kNormal, (int) code.length()).ToLocalChecked();

        // kompilera hela skiten, med allt inläst
        Local<Script> script = Script::Compile(context, source).ToLocalChecked();

        // kör första "tick" där vi sätter upp saker
        Local<Value> result = script->Run(context).ToLocalChecked();

        // kör servern nu
        uWS::Loop::defaultLoop()->run();
    }


    // fall through
    std::cout << "Done, bye!" << std::endl;
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
