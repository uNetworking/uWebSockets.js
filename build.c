#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* List of platform features */
#ifdef _WIN32
#define OS "win32"
#define IS_WINDOWS
#endif
#ifdef __linux
#define OS "linux"
#define IS_LINUX
#endif
#ifdef __APPLE__
#define OS "darwin"
#define IS_MACOS
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

/* Downloads headers, creates folders */
void prepare() {
    if (run("mkdir dist") || run("mkdir targets")) {
        return;
    }

    /* For all versions */
    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        run("curl -OJ https://nodejs.org/dist/%s/node-%s-headers.tar.gz", versions[i].name, versions[i].name);
        run("tar xzf node-%s-headers.tar.gz -C targets", versions[i].name);
        run("curl https://nodejs.org/dist/%s/win-x64/node.lib > targets/node-%s/node.lib", versions[i].name, versions[i].name);
    }
}

void build_lsquic(const char *arch) {
#ifndef IS_WINDOWS
    /* Build for arm64 and x64 for macOS */

#ifdef IS_MACOS
    if (arch == X64) {
        run("cd uWebSockets/uSockets/lsquic && mkdir -p arm64 && cd arm64 && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_OSX_ARCHITECTURES=arm64 -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic");
    } else if (arch == ARM64) {
        run("cd uWebSockets/uSockets/lsquic && mkdir -p x64 && cd x64 && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_OSX_ARCHITECTURES=x86_64 -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic");
    }
#else
    /* Linux */
    run("cd uWebSockets/uSockets/lsquic && mkdir -p %s && cd %s && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off .. && make lsquic", arch, arch);

#endif
    
    
    
#else
    /* Windows */

    /* Download zlib */
    run("curl -OL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz");
    run("tar xzf zlib-1.3.1.tar.gz");
    
    run("cd uWebSockets/uSockets/lsquic && cmake -DCMAKE_C_FLAGS=\"/DWIN32 /I..\\..\\..\\zlib-1.3.1\" -DZLIB_INCLUDE_DIR=..\\..\\..\\zlib-1.3.1 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off . && msbuild ALL_BUILD.vcxproj");
#endif
}

/* Build boringssl */
void build_boringssl(const char *arch) {

#ifdef IS_MACOS
    /* Only macOS uses cross-compilation */
    if (arch == X64) {
        run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 .. && make crypto ssl");
    } else if (arch == ARM64) {
        run("cd uWebSockets/uSockets/boringssl && mkdir -p arm64 && cd arm64 && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 .. && make crypto ssl");
    }
#endif
    
#ifdef IS_LINUX
    /* Build for x64 or arm/arm64 (depending on the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p %s && cd %s && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release .. && make crypto ssl", arch, arch);
#endif
    
#ifdef IS_WINDOWS
    /* Build for x64 (the host) */
    run("cd uWebSockets/uSockets/boringssl && mkdir -p x64 && cd x64 && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -GNinja .. && ninja crypto ssl");
#endif

}

/* Build for Unix systems */
void build(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {

    char *c_shared = "-DWIN32_LEAN_AND_MEAN -DLIBUS_USE_LIBUV -DLIBUS_USE_QUIC -I uWebSockets/uSockets/lsquic/include -I uWebSockets/uSockets/boringssl/include -pthread -DLIBUS_USE_OPENSSL -flto -O3 -c -fPIC -I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c uWebSockets/uSockets/src/crypto/*.c";
    char *cpp_shared = "-DWIN32_LEAN_AND_MEAN -DUWS_WITH_PROXY -DLIBUS_USE_LIBUV -DLIBUS_USE_QUIC -I uWebSockets/uSockets/boringssl/include -pthread -DLIBUS_USE_OPENSSL -flto -O3 -c -fPIC -std=c++20 -I uWebSockets/uSockets/src -I uWebSockets/src src/addon.cpp uWebSockets/uSockets/src/crypto/sni_tree.cpp";

    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        run("%s %s -I targets/node-%s/include/node", compiler, c_shared, versions[i].name);
        run("%s %s -I targets/node-%s/include/node", cpp_compiler, cpp_shared, versions[i].name);
        run("%s -pthread -flto -O3 *.o uWebSockets/uSockets/boringssl/%s/ssl/libssl.a uWebSockets/uSockets/boringssl/%s/crypto/libcrypto.a uWebSockets/uSockets/lsquic/%s/src/liblsquic/liblsquic.a -std=c++20 -shared %s -o dist/uws_%s_%s_%s.node", cpp_compiler, arch, arch, arch, cpp_linker, os, arch, versions[i].abi);
    }
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
    
    /* For all versions */
    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        run("cl /Zc:__cplusplus /MD /W3 /D WIN32_LEAN_AND_MEAN /D \"UWS_WITH_PROXY\" /D \"LIBUS_USE_LIBUV\" /D \"LIBUS_USE_QUIC\" /I uWebSockets/uSockets/lsquic/include /I uWebSockets/uSockets/lsquic/wincompat /I uWebSockets/uSockets/boringssl/include /D \"LIBUS_USE_OPENSSL\" /std:c++20 /I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/crypto/sni_tree.cpp "
            "uWebSockets/uSockets/src/eventing/*.c uWebSockets/uSockets/src/crypto/*.c /I targets/node-%s/include/node /I uWebSockets/src /EHsc "
            "/Ox /LD /Fedist/uws_win32_%s_%s.node src/addon.cpp advapi32.lib uWebSockets/uSockets/boringssl/x64/ssl/ssl.lib uWebSockets/uSockets/boringssl/x64/crypto/crypto.lib uWebSockets/uSockets/lsquic/src/liblsquic/Debug/lsquic.lib targets/node-%s/node.lib",
            versions[i].name, arch, versions[i].abi, versions[i].name);
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
    build_windows("clang",
          "clang++",
          "",
          OS,
          X64);
#else
#ifdef IS_MACOS

    /* Apple special case */
    build("clang -target x86_64-apple-macos12",
          "clang++ -stdlib=libc++ -target x86_64-apple-macos12",
          "-undefined dynamic_lookup",
          OS,
          X64);

    /* Try and build for arm64 macOS 12 */
    build("clang -target arm64-apple-macos12",
          "clang++ -stdlib=libc++ -target arm64-apple-macos12",
          "-undefined dynamic_lookup",
          OS,
          ARM64);

#else
    /* Linux does not cross-compile but picks whatever arch the host is on */
    build("clang-18",
          "clang++-18",
          "-static-libstdc++ -static-libgcc -s",
          OS,
          arch);

    /* If linux we also want arm64 */
    /*build("aarch64-linux-gnu-gcc",
        "aarch64-linux-gnu-g++",
        "-static-libstdc++ -static-libgcc -s",
        OS,
        ARM64);*/
#endif
#endif

    copy_files();
}

