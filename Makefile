default:
	$(CC) build.c
	./a.out || build.exe
upload_host:
	git fetch origin binaries:binaries
	git checkout binaries
	cp dist/*.node .
	git add *.node
	git commit -m "Manually updated host binaries"
	git push origin binaries
	git checkout master
