#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* List of platform features */
#if defined(_WIN32)

#define OS "win32"
#define IS_WINDOWS

#elif defined(__linux)

#define OS "linux"
#define IS_LINUX

#elif defined(__APPLE__)

#define OS "darwin"
#define IS_MACOS

#endif

/* ASAN vs. optimized build flags (used via C string literal concatenation).
 * OPT_FLAGS / LINK_FLAGS: inserted mid-string, so each definition starts with a space and ends with a space.
 * LINUX_LINK_EXTRAS: passed as a standalone argument, so no leading space.
 * MACOS_LINK_EXTRAS: appended after "-undefined dynamic_lookup", so ASAN variant starts with a space. */
#ifdef WITH_ASAN
#define OPT_FLAGS " -fsanitize=address -fno-omit-frame-pointer -g -O1 "
#define LINK_FLAGS " -fsanitize=address "
#define LINUX_LINK_EXTRAS "-fsanitize=address"
#define MACOS_LINK_EXTRAS " -fsanitize=address"
#else
#define OPT_FLAGS " -flto -O3 "
#define LINK_FLAGS " -flto -O3 "
#define LINUX_LINK_EXTRAS "-static-libstdc++ -static-libgcc -s"
#define MACOS_LINK_EXTRAS ""
#endif

const char *ARM = "arm";
const char *ARM64 = "arm64";
const char *X64 = "x64";

/* System, but with string replace */
int run(const char *cmd, ...) {
    char buf[2048];
    va_list args;
    va_start(args, cmd);
    vsprintf(buf, cmd, args);
    va_end(args);
    printf("--> %s\n\n", buf);
    return system(buf);
}

/* List of Node.js versions */
struct node_version {
    char *name;
    char *abi;
} versions[] = {
    {"v24.0.0", "137"},
    {"v20.0.0", "115"},
    {"v22.0.0", "127"},
    {"v25.0.0", "141"}
};
const unsigned int versionsQuantity = sizeof(versions) / sizeof(struct node_version);

/* Downloads headers, creates folders */
void prepare() {
    // see console output IMMEDIATELY for debugging purposes
    setbuf(stdout, 0);

    if (run("mkdir dist") || run("mkdir targets")) {
        printf("[NodeJS headers are already installed (v20,v22,v24,v25)]\n");
        return;
    }

    printf("\n<-- [Installing NodeJS headers] -->\n");
    /* For all versions curl is silenced '-s', but shows errors '-S' */
    for (unsigned int i = 0; i < versionsQuantity; i++) {

      run("curl -sSOJ https://nodejs.org/dist/%s/node-%s-headers.tar.gz",
          versions[i].name, versions[i].name);

      run("tar xzf node-%s-headers.tar.gz -C targets", versions[i].name);

      // windows requires node.lib
#if defined(IS_WINDOWS) 
      run("curl -sS https://nodejs.org/dist/%s/win-x64/node.lib -o "
          "targets/node-%s/node.lib",
          versions[i].name, versions[i].name);
#endif

        /* v8-fast-api-calls.h is missing from the Node.js header distribution; fetch the correct major version from the Node.js source tree */
      run("curl -sSfL "
          "https://raw.githubusercontent.com/nodejs/node/%s/deps/v8/include/v8-fast-api-calls.h "
          "-o targets/node-%s/include/node/v8-fast-api-calls.h",
          versions[i].name, versions[i].name);

    }
    printf("[Fetched NodeJS headers v20,v22,v24,v25]\n");
}

void build_lsquic(const char *arch) {
  printf("\n<-- [Started building lsquic: %s] -->\n", arch);
#ifndef IS_WINDOWS
    /* Build for arm64 and x64 for macOS */

#ifdef IS_MACOS
    if (arch == X64) {
      run("cd uWebSockets/uSockets/lsquic && mkdir -p arm64 && cd arm64 && "
          "cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "-DCMAKE_OSX_ARCHITECTURES=arm64 -DBORINGSSL_DIR=../boringssl "
          "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic");
    } else if (arch == ARM64) {
      run("cd uWebSockets/uSockets/lsquic && mkdir -p x64 && cd x64 && cmake "
          "-DCMAKE_POSITION_INDEPENDENT_CODE=ON "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "-DCMAKE_OSX_ARCHITECTURES=x86_64 -DBORINGSSL_DIR=../boringssl "
          "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic");
    }
#else
    /* Linux */
    run("cd uWebSockets/uSockets/lsquic && mkdir -p %s && cd %s && cmake "
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBORINGSSL_DIR=../boringssl "
        "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic",
        arch, arch);

#endif
    
    
    
#else
    /* Windows */

    /* Download zlib */
    run("curl -OL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz");
    run("tar xzf zlib-1.3.1.tar.gz");

    run("cd uWebSockets/uSockets/lsquic && cmake "
        "-DCMAKE_C_FLAGS=\"/Wv:18 /DWIN32 /wd4201 /I..\\..\\..\\zlib-1.3.1\" "
        "-DZLIB_INCLUDE_DIR=..\\..\\..\\zlib-1.3.1 "
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON "
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_C_COMPILER=clang "
        "-DCMAKE_CXX_COMPILER=clang++ -DBORINGSSL_DIR=../boringssl "
        "-DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off . "
        " && msbuild ALL_BUILD.vcxproj");
#endif
  printf("\n[Finished building lsquic: %s]\n", arch);
}

/* Build boringssl */
void build_boringssl(const char *arch) {
  printf("\n<-- [Started building boringssl: %s] -->\n", arch);
#ifdef IS_MACOS
    /* Only macOS uses cross-compilation */
    if (arch == X64) {
      run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && "
          "cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          ".. && make crypto ssl");
    } else if (arch == ARM64) {
      run("cd uWebSockets/uSockets/boringssl && mkdir -p arm64 && cd arm64 && "
          "cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 .. "
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "
          "&& make crypto ssl");
    }
#endif
    
#ifdef IS_LINUX
    /* Build for x64 or arm/arm64 (depending on the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p %s && cd %s &&"
        " cmake "
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON "
        "-DCMAKE_BUILD_TYPE=Release .. && "
        "make crypto ssl",
        arch, arch);
#endif
    
#ifdef IS_WINDOWS
    /* Build for x64 (the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && "
        "cmake "
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_C_COMPILER=clang "
        "-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -GNinja .. && "
        "ninja crypto ssl");
#endif
  printf("\n[Finished building boringssl: %s]\n", arch);
}

/* Build for Unix systems */
void build(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {
  printf("\n<-- [Started building uWebSockets.js: %s] -->\n", arch);
  char *c_shared =
      // includes
      "-I uWebSockets/uSockets/lsquic/include "
      "-I uWebSockets/uSockets/boringssl/include -pthread "
      "-I uWebSockets/uSockets/src "
      // flags
      "-DLIBUS_USE_OPENSSL" OPT_FLAGS
      "-DWIN32_LEAN_AND_MEAN -DLIBUS_USE_LIBUV -DLIBUS_USE_QUIC "
      "-c -fPIC "
      // sources
      "uWebSockets/uSockets/src/*.c "
      "uWebSockets/uSockets/src/eventing/*.c "
      "uWebSockets/uSockets/src/crypto/*.c";

  char *cpp_shared =
      // includes
      "-I uWebSockets/uSockets/src "
      "-I uWebSockets/src "
      "-I uWebSockets/uSockets/boringssl/include "
      // flags
      "-DWIN32_LEAN_AND_MEAN -DUWS_WITH_PROXY -DLIBUS_USE_LIBUV "
      "-DLIBUS_USE_QUIC "
      "-DLIBUS_USE_OPENSSL" OPT_FLAGS
      "-pthread -c -fPIC -std=c++20 "
      // sources
      "src/addon.cpp uWebSockets/uSockets/src/crypto/sni_tree.cpp";

  for (unsigned int i = 0; i < versionsQuantity; i++) {
    printf("[Compile C core: NodeJS %s]", versions[i].name);
    run("%s %s -I targets/node-%s/include/node", compiler, c_shared, versions[i].name);

    printf("[Compile uWebSockets.js: NodeJS %s]", versions[i].name);
    run("%s %s -I targets/node-%s/include/node", cpp_compiler, cpp_shared, versions[i].name);

    printf("[Link libraries: NodeJS %s]", versions[i].name);
    run("%s -pthread" LINK_FLAGS
        "*.o uWebSockets/uSockets/boringssl/%s/libssl.a "
        "uWebSockets/uSockets/boringssl/%s/libcrypto.a "
        "uWebSockets/uSockets/lsquic/%s/src/liblsquic/liblsquic.a "
        "-std=c++20 "
        "-shared %s -o dist/uws_%s_%s_%s.node",
        cpp_compiler, arch, arch, arch, cpp_linker, os, arch, versions[i].abi);
  }

  printf("\n[Finished building uWebSockets.js: %s]\n", arch);
}

void copy_files() {
#ifdef IS_WINDOWS
    run("copy \"src\\uws.js\" dist /Y");
#else
    run("cp src/uws.js dist/uws.js");
#endif
}

/* Special case for windows */
void build_windows(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {

  char *c_shared =
      // includes
      "-IuWebSockets/uSockets/lsquic/include "
      "-IuWebSockets/uSockets/lsquic/wincompat "
      "-IuWebSockets/uSockets/boringssl/include "
      "-IuWebSockets/uSockets/src "
      //flags
      "-DWIN32_LEAN_AND_MEAN "
      "-DLIBUS_USE_LIBUV "
      "-DLIBUS_USE_QUIC "
      "-DLIBUS_USE_OPENSSL "
      "-O3 -c "
      // sources 
      "uWebSockets/uSockets/src/*.c "
      "uWebSockets/uSockets/src/eventing/*.c "
      "uWebSockets/uSockets/src/crypto/*.c";
  char *cpp_shared =
      // includes
      "-IuWebSockets/uSockets/lsquic/include "
      "-IuWebSockets/uSockets/lsquic/wincompat "
      "-IuWebSockets/uSockets/boringssl/include "
      "-IuWebSockets/uSockets/src "
      "-IuWebSockets/src "
      //flags
      "-DWIN32_LEAN_AND_MEAN "
      "-DUWS_WITH_PROXY "
      "-DLIBUS_USE_LIBUV "
      "-DLIBUS_USE_QUIC "
      "-DLIBUS_USE_OPENSSL "
      "-O3 -c -std=c++20 "
      // sources
      "src/addon.cpp "
      "uWebSockets/uSockets/src/crypto/sni_tree.cpp";

  for (unsigned int i = 0; i < versionsQuantity; i++) {

    printf("[Compile C core: NodeJS %s]", versions[i].name);
    run("%s %s -Itargets/node-%s/include/node", compiler, c_shared, versions[i].name);

    printf("[Compile uWebSockets.js: NodeJS %s]", versions[i].name);
    run("%s %s -Itargets/node-%s/include/node", cpp_compiler, cpp_shared, versions[i].name);

    printf("[Link libraries: NodeJS %s]", versions[i].name);
    run("%s -O3 *.o uWebSockets/uSockets/boringssl/%s/ssl.lib "
        "uWebSockets/uSockets/boringssl/%s/crypto.lib "
        "uWebSockets/uSockets/lsquic/src/liblsquic/Debug/lsquic.lib "
        "targets/node-%s/node.lib "
        "-ladvapi32 -std=c++20 -shared -o "
        "dist/uws_win32_%s_%s.node",
        cpp_compiler, arch, arch, versions[i].name, arch, versions[i].abi);
  }
}

int main() {
    printf("[Preparing]\n");
    prepare();
    printf("\n[Building]\n");
    
    const char *arch = X64;
#ifdef __arm__
    arch = ARM;
#endif
#ifdef __aarch64__
    arch = ARM64;
#endif

#ifdef IS_MACOS
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
