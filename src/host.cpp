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

        // add sign of stand-alone
        global->Set(isolate, "runtime", String::NewFromUtf8(isolate, "µWebSockets.js", NewStringType::kNormal).ToLocalChecked());

        // console namespace
        Local<ObjectTemplate> console = ObjectTemplate::New(isolate);
        // console.log
        console->Set(String::NewFromUtf8(isolate, "log", NewStringType::kNormal).ToLocalChecked(),
        FunctionTemplate::New(isolate, print));

        // uWS namespace
        Local<ObjectTemplate> uWS = ObjectTemplate::New(isolate);

        global->Set(String::NewFromUtf8(isolate, "console", NewStringType::kNormal).ToLocalChecked(),
        console);
        global->Set(String::NewFromUtf8(isolate, "uWS", NewStringType::kNormal).ToLocalChecked(),
        uWS);

        // vi skapar ett context som håller det globala objektet
        Local<Context> context = Context::New(isolate, nullptr, global);
        Context::Scope context_scope(context);

        // register µWS features under uWS namespace
        Main(Local<Object>::Cast(context->Global()->Get(String::NewFromUtf8(isolate, "uWS", NewStringType::kNormal).ToLocalChecked())));

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
