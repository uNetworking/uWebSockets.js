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

/* System, but with string replace */
int run(const char *cmd, ...) {
    char buf[512];
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
    {"v10.0.0", "64"},
    {"v11.1.0", "67"}
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

/* Build for Unix systems */
void build(char *compiler, char *cpp_compiler, char *cpp_linker, char *os, char *arch) {
    char *c_shared = "-DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c";
    char *cpp_shared = "-DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -std=c++17 -I uWebSockets/uSockets/src -I uWebSockets/src src/addon.cpp";

    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        run("%s %s -I targets/node-%s/include/node", compiler, c_shared, versions[i].name);
        run("%s %s -I targets/node-%s/include/node", cpp_compiler, cpp_shared, versions[i].name);
        run("%s %s %s -o dist/uws_%s_%s_%s.node", cpp_compiler, "-flto -O3 *.o -std=c++17 -shared", cpp_linker, os, arch, versions[i].abi);
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
void build_windows(char *arch) {
    /* For all versions */
    for (unsigned int i = 0; i < sizeof(versions) / sizeof(struct node_version); i++) {
        run("cl /D \"LIBUS_USE_LIBUV\" /std:c++17 /I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c "
            "uWebSockets/uSockets/src/eventing/*.c /I targets/node-%s/include/node /I uWebSockets/src /EHsc "
            "/Ox /LD /Fedist/uws_win32_%s_%s.node src/addon.cpp targets/node-%s/node.lib",
            versions[i].name, arch, versions[i].abi, versions[i].name);
    }
}

int main() {
    printf("[Preparing]\n");
    prepare();
    printf("\n[Building]\n");

#ifdef IS_WINDOWS
    build_windows("x64");
#else
#ifdef IS_MACOS
    /* Apple special case */
    build("clang -mmacosx-version-min=10.7",
          "clang++ -stdlib=libc++ -mmacosx-version-min=10.7",
          "-undefined dynamic_lookup",
          OS,
          "x64");
#else
    /* Linux */
    build("clang",
          "clang++",
          "-static-libstdc++ -static-libgcc -s",
          OS,
          "x64");

    /* If linux we also want arm64 */
    build("aarch64-linux-gnu-gcc", "aarch64-linux-gnu-g++", "-static-libstdc++ -static-libgcc -s", OS, "arm64");
#endif
#endif

    copy_files();
}

