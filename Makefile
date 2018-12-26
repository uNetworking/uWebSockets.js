C_SHARED := -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -I uWebSockets/uSockets/src uWebSockets/uSockets/src/*.c uWebSockets/uSockets/src/eventing/*.c

CPP_SHARED := -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -std=c++17 -I uWebSockets/uSockets/src -I uWebSockets/src src/addon.cpp

C_OSX := -mmacosx-version-min=10.7

CPP_OSX := -stdlib=libc++ $(C_OSX)

default:
	make targets
	NODE=targets/node-v10.0.0 ABI=64 make `(uname -s)`
	NODE=targets/node-v11.1.0 ABI=67 make `(uname -s)`
	for f in dist/*.node; do chmod +x $$f; done
targets:
	mkdir targets
	curl https://nodejs.org/dist/v10.0.0/node-v10.0.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v11.1.0/node-v11.1.0-headers.tar.gz | tar xz -C targets
Linux:
	clang $(C_SHARED) -I $$NODE/include/node
	clang++ $(CPP_SHARED) -I $$NODE/include/node
	clang++ -flto -O3 *.o -std=c++17 -shared -static-libstdc++ -static-libgcc -s -o dist/uws_linux_$$ABI.node
Darwin:
	gcc $(C_OSX) $(C_SHARED) -I $$NODE/include/node
	g++ $(CPP_OSX) $(CPP_SHARED) -I $$NODE/include/node
	g++ $(CPP_OSX) -flto -O3 *.o -std=c++17 -shared -undefined dynamic_lookup -o dist/uws_darwin_$$ABI.node
.PHONY: clean
clean:
	rm -f dist/README.md
	rm -f dist/LICENSE
	rm -f dist/uws_*.node
	rm -f dist/uws.js
	rm -rf dist/src
	rm -rf targets
