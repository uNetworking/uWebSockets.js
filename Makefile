Windows:
	$(CC) build.c
	./a.out || build.exe

clean:
	rm -rf dist
	rm -rf headers
	rm -rf targets
	rm -rf objects
	rm -f  *.out

clean-windows:
	rmdir /S /Q dist
	rmdir /S /Q headers
	rmdir /S /Q targets
	rmdir /S /Q objects
	del *.out