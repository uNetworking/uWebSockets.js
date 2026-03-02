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

int addon_only = 0;
int latest_only = 0;
int debug_mode = 0;
int disable_http3 = 0;
char *selected_version = NULL;

int exists(const char *fname) {
    FILE *file;
    if ((file = fopen(fname, "r"))) {
        fclose(file);
        return 1;
    }
    return 0;
}

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

int arch_is(const char *arch, const char *expected) {
    return strcmp(arch, expected) == 0;
}

const char *windows_arch_name(const char *arch) {
    if (arch_is(arch, X64)) {
        return "x64";
    }
    if (arch_is(arch, ARM64)) {
        return "arm64";
    }
    if (arch_is(arch, ARM)) {
        return "arm";
    }
    return NULL;
}

const char *windows_build_dir(const char *arch) {
    if (arch_is(arch, X64)) {
        return "win-x64";
    }
    if (arch_is(arch, ARM64)) {
        return "win-arm64";
    }
    return NULL;
}

/* Downloads headers, creates folders */
void prepare(const char *windows_lib_arch) {
#ifdef IS_WINDOWS
    if (run("if not exist dist mkdir dist") || run("if not exist targets mkdir targets")) {
        return;
    }
#else
    if (run("mkdir -p dist") || run("mkdir -p targets")) {
        return;
    }
#endif

    /* For all versions */
    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        if (selected_version && strcmp(versions[i].name, selected_version)) {
            continue;
        }

        char path[256];
        sprintf(path, "node-%s-headers.tar.gz", versions[i].name);
        if (!exists(path)) {
            run("curl -OJ https://nodejs.org/dist/%s/node-%s-headers.tar.gz", versions[i].name, versions[i].name);
        }
        run("tar xzf node-%s-headers.tar.gz -C targets", versions[i].name);

        sprintf(path, "targets/node-%s/node.lib", versions[i].name);
        if (!exists(path)) {
            run("curl https://nodejs.org/dist/%s/win-%s/node.lib > targets/node-%s/node.lib", versions[i].name, windows_lib_arch, versions[i].name);
        }
    
        if (latest_only) {
            break;
        }
    }
}

void build_lsquic(const char *arch) {
    if (disable_http3) {
        return;
    }
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
    if (!exists("zlib-1.3.1.tar.gz")) {
        run("curl -OL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz");
    }

    run("tar xzf zlib-1.3.1.tar.gz");
    run("cd uWebSockets/uSockets/lsquic && cmake -DCMAKE_C_FLAGS=\"-DWIN32 -I..\\..\\..\\zlib-1.3.1\" -DZLIB_INCLUDE_DIR=..\\..\\..\\zlib-1.3.1 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DBORINGSSL_DIR=../boringssl -DCMAKE_BUILD_TYPE=Release -DLSQUIC_BIN=Off -GNinja . && ninja");
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
    run("cd uWebSockets/uSockets/boringssl && if not exist x64 mkdir x64 && cd x64 && cmake -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_BUILD_TYPE=Release -GNinja .. && ninja crypto ssl");
#endif

}

/* Build for Unix systems */
void build(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, const char *arch) {

    const char *opt_flags = debug_mode ? "-g -O0" : "-O3";
    const char *http3_flags = disable_http3 ? "-DUWS_NO_HTTP3" : "-DLIBUS_USE_QUIC";
    const char *http3_include = disable_http3 ? "" : " -I uWebSockets/uSockets/lsquic/include";

    char c_shared[1024];
    char cpp_shared[1024];
    snprintf(c_shared, sizeof(c_shared), "-DWIN32_LEAN_AND_MEAN -DLIBUS_USE_LIBUV -DUWS_USE_LIBDEFLATE %s -I uWebSockets/libdeflate -I uWebSockets/uSockets/boringssl/include -pthread -DLIBUS_USE_OPENSSL -flto %s%s -c -fPIC -I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c uWebSockets/uSockets/src/crypto/*.c", http3_flags, opt_flags, http3_include);
    snprintf(cpp_shared, sizeof(cpp_shared), "-DWIN32_LEAN_AND_MEAN -DUWS_WITH_PROXY -DUWS_USE_LIBDEFLATE -DLIBUS_USE_LIBUV %s -I uWebSockets/uSockets/boringssl/include -I uWebSockets/libdeflate -pthread -DLIBUS_USE_OPENSSL -flto %s%s -c -fPIC -std=c++20 -I uWebSockets/uSockets/src -I uWebSockets/src src/addon.cpp uWebSockets/uSockets/src/crypto/sni_tree.cpp -static -lbrotlienc", http3_flags, opt_flags, http3_include); // Static link so we don't need to depend
    
    char lsquic_libs[512] = "";
    if (!disable_http3) {
        snprintf(lsquic_libs, sizeof(lsquic_libs), " uWebSockets/uSockets/lsquic/%s/src/liblsquic/liblsquic.a", arch);
    }

    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        if (selected_version && strcmp(versions[i].name, selected_version)) {
            continue;
        }

        if(!addon_only) {
            run("%s %s -I targets/node-%s/include/node", compiler, c_shared, versions[i].name);
            run("%s %s -I targets/node-%s/include/node", cpp_compiler, cpp_shared, versions[i].name);
        }
        run("%s -pthread -flto %s *.o uWebSockets/uSockets/boringssl/%s/ssl/libssl.a uWebSockets/uSockets/boringssl/%s/crypto/libcrypto.a%s -I uWebSockets/libdeflate -std=c++20 -shared %s -o dist/akeno_%s_%s_%s.node", cpp_compiler, opt_flags, arch, arch, lsquic_libs, cpp_linker, os, arch, versions[i].abi);

        if(addon_only || latest_only) {
            break; // Only build for one version
        }
    }
}

void copy_files() {
#ifdef IS_WINDOWS
    run("copy \"src\\uws.js\" dist /Y");
#else
    run("cp src/uws.js dist/uws.js");
#endif
}

/* Special case for windows (MSVC) */
void build_windows(const char *os, const char *arch) {
    const char *http3_defs = disable_http3 ? "/DUWS_NO_HTTP3" : "/DLIBUS_USE_QUIC";
    const char *http3_includes = disable_http3 ? "" : " /I uWebSockets/uSockets/lsquic/include /I uWebSockets/uSockets/lsquic/wincompat";
    const char *http3_libs = disable_http3 ? "" : " uWebSockets\\uSockets\\lsquic\\src\\liblsquic\\Debug\\lsquic.lib";
    const char *opt_flags = debug_mode ? "/Zi /Od" : "/O2";

    char c_shared[1800];
    char cpp_shared[1800];

    snprintf(c_shared, sizeof(c_shared), "/nologo /c /FS /MT /DWIN32_LEAN_AND_MEAN /DLIBUS_USE_LIBUV /DUWS_USE_LIBDEFLATE %s /I uWebSockets/uSockets/boringssl/include /I uWebSockets/libdeflate /DLIBUS_USE_OPENSSL %s%s /I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c uWebSockets/uSockets/src/crypto/*.c", http3_defs, opt_flags, http3_includes);
    snprintf(cpp_shared, sizeof(cpp_shared), "/nologo /c /FS /MT /std:c++20 /EHsc /DWIN32_LEAN_AND_MEAN /DUWS_WITH_PROXY /DLIBUS_USE_LIBUV /DUWS_USE_LIBDEFLATE %s /I uWebSockets/uSockets/boringssl/include /I uWebSockets/libdeflate /DLIBUS_USE_OPENSSL %s%s /I uWebSockets/uSockets/src /I uWebSockets/src src/addon.cpp uWebSockets/uSockets/src/crypto/sni_tree.cpp", http3_defs, opt_flags, http3_includes);

    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        if (selected_version && strcmp(versions[i].name, selected_version)) {
            continue;
        }

        if (!addon_only) {
            run("del /Q *.obj >NUL 2>&1");
            run("cl %s /I targets/node-%s/include/node", c_shared, versions[i].name);
            run("cl %s /I targets/node-%s/include/node", cpp_shared, versions[i].name);
        }

        run("link /NOLOGO /DLL /OUT:dist\\akeno_%s_%s_%s.node *.obj uWebSockets\\uSockets\\boringssl\\%s\\ssl\\ssl.lib uWebSockets\\uSockets\\boringssl\\%s\\crypto\\crypto.lib%s targets\\node-%s\\node.lib BrotliEnc.lib BrotliCommon.lib Ws2_32.lib Crypt32.lib Bcrypt.lib Iphlpapi.lib Userenv.lib Psapi.lib Advapi32.lib", os, arch, versions[i].abi, arch, arch, http3_libs, versions[i].name);

        if (addon_only || latest_only) {
            break;
        }
    }
}

int main(int argc, char **argv) {
#ifdef IS_WINDOWS
    printf("[Warning] Building Akeno-uWS for Windows is not supported and Akeno does not support Windows. Any Windows build is considered experimental/unsupported and can break or not be up to expectations. Use at your own risk\n\n");
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--addon-only")) {
            addon_only = 1;
            printf("Only building for one Node.js version and skipping preparation, assuming you have built before\n");
        }
        if (!strcmp(argv[i], "--latest-only")) {
            latest_only = 1;
            printf("Only building for one Node.js version.\n");
        }
        if (!strcmp(argv[i], "--debug")) {
            debug_mode = 1;
            printf("Debug build enabled (-g -O0).\n");
        }
        if (!strcmp(argv[i], "--no-http3")) {
            disable_http3 = 1;
            printf("HTTP/3 and QUIC support disabled.\n");
        }
        if (strncmp(argv[i], "--version=", 10) == 0) {
            selected_version = argv[i] + 10;
        } else if (!strcmp(argv[i], "--version") && i + 1 < argc) {
            selected_version = argv[++i];
        }
    }

    const char *arch = X64;
#ifdef __arm__
    arch = ARM;
#endif
#ifdef __aarch64__
    arch = ARM64;
#endif

    if (!addon_only) {
        printf("[Preparing]\n");
        prepare("x64");
    }
    printf("\n[Building]\n");

#ifdef IS_MACOS
    /* If we are macOS, build both arm64 and x64 */
    if (!addon_only) {
        build_boringssl(X64);
        build_boringssl(ARM64);
        if (!disable_http3) {
            build_lsquic(X64);
            build_lsquic(ARM64);
        }
    }
#else
    /* For other platforms we simply compile the host */
    if (!addon_only) {
        build_boringssl(arch);
        if (!disable_http3) {
            build_lsquic(arch);
        }
    }
#endif


#ifdef IS_WINDOWS
    build_windows(OS, X64);
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
    /* Linux does not cross-compile but picks whatever arch the host is on (we run on both x64 and ARM64) */
    build("clang-18",
          "clang++-18",
          "-static-libstdc++ -static-libgcc -s",
          OS,
          arch);
#endif
#endif

    copy_files();
}
