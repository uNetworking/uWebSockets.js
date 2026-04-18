#include "./build-conf.h"
/* Downloads headers, creates folders */

void nodejs_headers(const char* version) {
run(
  "curl -sSL \"https://nodejs.org/dist/%s/node-%s-headers.tar.gz\" -o tmp/node-%s-headers.tar.gz"
  " && "
  "tar xzf tmp/node-%s-headers.tar.gz -C targets", 
  version, version, version, version

);
run(  
    
  /*fetch v8 fast-api manually*/
  "curl -sSfL \"https://raw.githubusercontent.com/nodejs/node/%s/deps/v8/include/v8-fast-api-calls.h\" -o \"targets/node-%s/include/node/v8-fast-api-calls.h\"", version, version
);
  
#ifdef IS_WINDOWS // fetch node.lib
run(
    "curl -sSL \"https://nodejs.org/dist/%s/win-x64/node.lib\" -o \"targets/node-%s/node.lib\"",
    version, version
);
#endif
}

void prepare() {
    // see console output IMMEDIATELY for debugging purposes
    setbuf(stdout, 0);

    // many syscall, I know...
    run("mkdir tmp");
    run("mkdir dist");
    for (unsigned int i = 0; i < versionsQuantity; i++) {
      run("mkdir \"tmp/c-%s\"", versions[i].abi);
      run("mkdir \"tmp/cpp-%s\"", versions[i].abi);
    }

    if (run("mkdir targets")) {
      printf("[NodeJS headers are already installed (v20,v22,v24,v25)]\n");
      return ;
    }
    printf("\n<-- [Installing NodeJS headers] -->\n");

    for (unsigned char i = 0; i < versionsQuantity; i++) {
      nodejs_headers(versions[i].name);
    }

    printf("[Fetched NodeJS headers v20,v22,v24,v25]\n");
}

void build_lsquic(const char *arch) {
printf("<-- [Build lsquic] -->\n");
#ifndef IS_WINDOWS
    /* Build for arm64 and x64 for macOS */

#ifdef IS_MACOS
    if (arch == X64) {
      run("cd uWebSockets/uSockets/lsquic && mkdir -p arm64 && cd arm64 && "
          "cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "-DCMAKE_OSX_ARCHITECTURES=arm64 -DBORINGSSL_DIR=../boringssl "
          "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make -j%i lsquic",
          threads_quantity);
    } else if (arch == ARM64) {
      run("cd uWebSockets/uSockets/lsquic && mkdir -p x64 && cd x64 && cmake "
          "-DCMAKE_POSITION_INDEPENDENT_CODE=ON "
           "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "-DCMAKE_OSX_ARCHITECTURES=x86_64 -DBORINGSSL_DIR=../boringssl "
          "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make -j%i lsquic",
          threads_quantity);
    }
#else
    /* Linux */
    run("cd uWebSockets/uSockets/lsquic && mkdir -p %s && cd %s && cmake -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make -j%i lsquic", arch, arch, threads_quantity);

#endif
    
    
    
#else
    /* Windows */

    /* Download zlib */
    run("curl -sSOL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz");
    run("tar xzf zlib-1.3.1.tar.gz");
    
    run("cd uWebSockets/uSockets/lsquic && cmake -DCMAKE_C_FLAGS=\"/Wv:18 /DWIN32 /wd4201 /I..\\..\\..\\zlib-1.3.1\" -DZLIB_INCLUDE_DIR=..\\..\\..\\zlib-1.3.1 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off . && msbuild ALL_BUILD.vcxproj /m:%i", threads_quantity);
#endif
printf("[Successfully built lsquic]\n");
}

/* Build boringssl */
void build_boringssl(const char *arch) {
printf("<-- [Build boringssl] -->\n");
#ifdef IS_MACOS
    /* Only macOS uses cross-compilation */
    if (arch == X64) {
      run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && "
          "cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          ".. && make -j%i crypto ssl",
          threads_quantity);
    } else if (arch == ARM64) {
      run("cd uWebSockets/uSockets/boringssl && mkdir -p arm64 && cd arm64 && "
          "cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 .. "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "&& make -j%i crypto ssl",
          threads_quantity);
    }
#endif
    
#ifdef IS_LINUX
    /* Build for x64 or arm/arm64 (depending on the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p %s && cd %s && cmake -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release .. && make -j%i crypto ssl", arch, arch, threads_quantity);
#endif
    
#ifdef IS_WINDOWS
    /* Build for x64 (the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && cmake "
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_C_COMPILER=clang "
        "-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -GNinja .. && "
        "ninja -j%i crypto ssl",
        threads_quantity);
#endif

printf("[Successfully built boringssl]\n");
}

/* Build for Unix systems */
#if !defined(IS_WINDOWS)
void build(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {
  printf("\n<-- [Building uWebSockets.js] -->\n");
  char *c_shared =
      "-DWIN32_LEAN_AND_MEAN -DLIBUS_USE_LIBUV -DLIBUS_USE_QUIC "
      " -I ../../uWebSockets/uSockets/lsquic/include "
      " -I ../../uWebSockets/uSockets/boringssl/include -pthread "
      " -DLIBUS_USE_OPENSSL" OPT_FLAGS
      " -c -fPIC -I ../../uWebSockets/uSockets/src "
      " ../../uWebSockets/uSockets/src/*.c "
      " ../../uWebSockets/uSockets/src/eventing/*.c "
      " ../../uWebSockets/uSockets/src/crypto/*.c";
  char *cpp_shared =
      "-DWIN32_LEAN_AND_MEAN -DUWS_WITH_PROXY -DLIBUS_USE_LIBUV "
      " -DLIBUS_USE_QUIC -I ../../uWebSockets/uSockets/boringssl/include "
      " -pthread "
      " -DLIBUS_USE_OPENSSL" OPT_FLAGS
      " -c -fPIC -std=c++20 -I ../../uWebSockets/uSockets/src"
      " -I ../../uWebSockets/src"
      " ../../src/addon.cpp ../../uWebSockets/uSockets/src/crypto/sni_tree.cpp";

  pid_t pids[versionsQuantity];
  for (unsigned int i = 0; i < versionsQuantity; i++) {
    // NodeJS versions ~ 4 and GitHub Actions provide 4 threads
    pids[i] = fork();
    if (pids[i] == 0) {
      // different abi = no race condition in parallel
      run("cd tmp/c-%s && %s %s -I ../../targets/node-%s/include/node",
          versions[i].abi, compiler, c_shared, versions[i].name);
      run("cd tmp/cpp-%s && %s %s -I ../../targets/node-%s/include/node",
          versions[i].abi, cpp_compiler, cpp_shared, versions[i].name);

      run("%s -pthread" LINK_FLAGS " tmp/c-%s/*.o tmp/cpp-%s/*.o "
          "uWebSockets/uSockets/boringssl/%s/libssl.a "
          "uWebSockets/uSockets/boringssl/%s/libcrypto.a "
          "uWebSockets/uSockets/lsquic/%s/src/liblsquic/liblsquic.a "
          "-std=c++20 -shared %s -o dist/uws_%s_%s_%s.node",
          cpp_compiler, versions[i].abi, versions[i].abi, arch, arch, arch,
          cpp_linker, os, arch, versions[i].abi);
      exit(0);
    }
  }
  for (unsigned int i = 0; i < versionsQuantity; i++) waitpid(pids[i], 0, 0);
  printf("\n[Finished building uWebSockets.js]\n");
}
#endif
void copy_files() {
#ifdef IS_WINDOWS
    run("copy \"src\\uws.js\" dist /Y");
#else
    run("cp src/uws.js dist/uws.js");
#endif
}

#ifdef IS_WINDOWS
/* Special case for windows */
void build_windows(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {
  printf("\n<-- [Building uWebSockets.js] -->\n");
  char *c_shared =
      "-DWIN32_LEAN_AND_MEAN -DLIBUS_USE_LIBUV -DLIBUS_USE_QUIC "
      "-I../../uWebSockets/uSockets/lsquic/include "
      "-I../../uWebSockets/uSockets/lsquic/wincompat "
      "-I../../uWebSockets/uSockets/boringssl/include -DLIBUS_USE_OPENSSL -O3 -c "
      "-I../../uWebSockets/uSockets/src ../../uWebSockets/uSockets/src/*.c "
      "../../uWebSockets/uSockets/src/eventing/*.c "
      "../../uWebSockets/uSockets/src/crypto/*.c";
  char *cpp_shared =
      "-DWIN32_LEAN_AND_MEAN -DUWS_WITH_PROXY -DLIBUS_USE_LIBUV "
      "-DLIBUS_USE_QUIC -I../../uWebSockets/uSockets/lsquic/include "
      "-I../../uWebSockets/uSockets/lsquic/wincompat "
      "-I../../uWebSockets/uSockets/boringssl/include -DLIBUS_USE_OPENSSL -O3 -c "
      "-std=c++20 -I../../uWebSockets/uSockets/src -I../../uWebSockets/src ../../src/addon.cpp "
      "../../uWebSockets/uSockets/src/crypto/sni_tree.cpp";

  for (unsigned int i = 0; i < versionsQuantity; i++) {
    // uSockets depends on libuv
    run("cd \"tmp\\c-%s\" && %s %s -I../../targets/node-%s/include/node", versions[i].abi, compiler, c_shared,
        versions[i].name);
    run("cd \"tmp\\cpp-%s\" && %s %s -I../../targets/node-%s/include/node", versions[i].abi, cpp_compiler, cpp_shared,
        versions[i].name);
    run("%s -O3 " " ./tmp/c-%s/*.o ./tmp/cpp-%s/*.o " "./uWebSockets/uSockets/boringssl/%s/ssl.lib "
        "./uWebSockets/uSockets/boringssl/%s/crypto.lib "
        "./uWebSockets/uSockets/lsquic/src/liblsquic/Debug/lsquic.lib "
        "./targets/node-%s/node.lib -ladvapi32 -std=c++20 -shared -o dist/uws_win32_%s_%s.node ", cpp_compiler, versions[i].abi, versions[i].abi,
        arch, arch, versions[i].name, arch, versions[i].abi);
  }
  printf("\n[Finished building uWebSockets.js]\n");
}
#endif

int main(int argc, const char* argv[]) {
    printf("<-- ENTRY POINT!!! -->\n[Parallel threads available: %i]\n", threads_quantity);
    threads_quantity = get_cpu_count();
    printf("[Preparing]\n");
    prepare();
    printf("\n[Building]\n");
    const char *arch = X64;
    printf("build.exe was started with %i arguments.\n", argc);
    if(argc != 2 || memcmp("prebuilt", argv[1], 8) != 0){
#ifdef __arm__
    arch = ARM;
#endif
#ifdef __aarch64__
    arch = ARM64;
#endif

#ifdef IS_MACOS
    putenv("MACOSX_DEPLOYMENT_TARGET=12.0");
    /* If we are macOS, build both arm64 and x64 */
    build_boringssl(X64);
    build_boringssl(ARM64);
    build_lsquic(X64);
    build_lsquic(ARM64);
#else
    /* For other platforms we simply compile the host */
    build_boringssl(arch);
    build_lsquic(arch);
#endif
    }

#ifdef IS_WINDOWS
    /* We can use clang, but we currently do use cl.exe still */
    build_windows("clang -fms-runtime-lib=static",
          "clang++ -fms-runtime-lib=static",
          "",
          OS,
          X64);
#else
#ifdef IS_MACOS
    /* Apple special case */
    build("clang -target x86_64-apple-macos12",
          "clang++ -stdlib=libc++ -target x86_64-apple-macos12",
          "-undefined dynamic_lookup" MACOS_LINK_EXTRAS,
          OS,
          X64);

    /* Try and build for arm64 macOS 12 */
    build("clang -target arm64-apple-macos12",
          "clang++ -stdlib=libc++ -target arm64-apple-macos12",
          "-undefined dynamic_lookup" MACOS_LINK_EXTRAS,
          OS,
          ARM64);

#else
    /* Linux does not cross-compile but picks whatever arch the host is on (we run on both x64 and ARM64) */
    build("clang-18",
          "clang++-18",
          LINUX_LINK_EXTRAS,
          OS,
          arch);
#endif
#endif

    copy_files();
}
