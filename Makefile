C_SHARED := -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -I ../uWebSockets/uSockets/src ../uWebSockets/uSockets/src/*.c ../uWebSockets/uSockets/src/eventing/*.c

CPP_SHARED := -DLIBUS_USE_LIBUV -flto -O3 -c -fPIC -std=c++17 -I ../uWebSockets/uSockets/src -I ../uWebSockets/src src/addon.cpp

CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup

default:
	make targets
	NODE=targets/node-v8.1.2 ABI=57 make `(uname -s)`
	NODE=targets/node-v9.2.0 ABI=59 make `(uname -s)`
	NODE=targets/node-v10.0.0 ABI=64 make `(uname -s)`
	NODE=targets/node-v11.1.0 ABI=67 make `(uname -s)`
	for f in dist/*.node; do chmod +x $$f; done
targets:
	mkdir targets
	curl https://nodejs.org/dist/v8.1.2/node-v8.1.2-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v9.2.0/node-v9.2.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v10.0.0/node-v10.0.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v11.1.0/node-v11.1.0-headers.tar.gz | tar xz -C targets
Linux:
	gcc $(C_SHARED)
	g++ $(CPP_SHARED) -I $$NODE/include/node
	g++ -flto -O3 *.o -std=c++17 -shared -static-libstdc++ -static-libgcc -s -o dist/uws_linux_$$ABI.node
Darwin:
	g++ $(CPP_SHARED) $(CPP_OSX) -I $$NODE/include/node -o dist/uws_darwin_$$ABI.node
.PHONY: clean
clean:
	rm -f dist/README.md
	rm -f dist/LICENSE
	rm -f dist/uws_*.node
	rm -f dist/uws.js
	rm -rf dist/src
	rm -rf targets
