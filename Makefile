default:
	$(CC) build.c -o build.exe
	./build.exe
prebuilt:
	$(CC) build.c -o build.exe
	./build.exe prebuilt
prebuilt-asan:
	$(CC) build.c -DWITH_ASAN -o build.exe
	./build.exe prebuilt
upload_host:
	git fetch --depth 1 origin binaries:binaries
	git checkout binaries
	cp dist/*.node .
	git add *.node
	git commit -m "Manually updated host binaries"
	git push origin binaries
	git checkout master
