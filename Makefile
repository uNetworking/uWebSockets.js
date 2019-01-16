# Note: for Windows, run "nmake Windows" in x64 developer terminal

C_SHARED = -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c
CPP_SHARED = -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -std=c++17 -I uWebSockets/uSockets/src -I uWebSockets/src src/addon.cpp
C_OSX = -mmacosx-version-min=10.7
CPP_OSX = -stdlib=libc++ $(C_OSX)

Unices:
	make targets
	NODE=targets/node-v10.0.0 ABI=64 make `(uname -s)`
	NODE=targets/node-v11.1.0 ABI=67 make `(uname -s)`
	for f in dist/*.node; do chmod +x $$f; done

targets:
# Node.js 10
	mkdir targets
	curl -OJ https://nodejs.org/dist/v10.0.0/node-v10.0.0-headers.tar.gz
	tar xzf node-v10.0.0-headers.tar.gz -C targets
	curl https://nodejs.org/dist/v10.0.0/win-x64/node.lib > targets/node-v10.0.0/node.lib
# Node.js 11
	curl -OJ https://nodejs.org/dist/v11.1.0/node-v11.1.0-headers.tar.gz
	tar xzf node-v11.1.0-headers.tar.gz -C targets
	curl https://nodejs.org/dist/v11.1.0/win-x64/node.lib > targets/node-v11.1.0/node.lib

Linux:
	clang $(C_SHARED) -I $$NODE/include/node
	clang++ $(CPP_SHARED) -I $$NODE/include/node
	clang++ -flto -O3 *.o -std=c++17 -shared -static-libstdc++ -static-libgcc -s -o dist/uws_linux_$$ABI.node

Darwin:
	clang $(C_OSX) $(C_SHARED) -I $$NODE/include/node
	clang++ $(CPP_OSX) $(CPP_SHARED) -I $$NODE/include/node
	clang++ $(CPP_OSX) -flto -O3 *.o -std=c++17 -shared -undefined dynamic_lookup -o dist/uws_darwin_$$ABI.node

Windows:
	nmake targets
	cl /D "LIBUS_USE_LIBUV" /std:c++17 /I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c /I targets/node-v10.0.0/include/node /I uWebSockets/src /EHsc /Ox /LD /Fedist/uws_win32_64.node src/addon.cpp targets/node-v10.0.0/node.lib
	cl /D "LIBUS_USE_LIBUV" /std:c++17 /I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c /I targets/node-v11.1.0/include/node /I uWebSockets/src /EHsc /Ox /LD /Fedist/uws_win32_67.node src/addon.cpp targets/node-v11.1.0/node.lib
