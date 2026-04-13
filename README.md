<p align="center">
  <img src="https://github.com/user-attachments/assets/a742cb99-7423-4be7-8e2c-f12b8324fae7" width="900"/>
</p>

# Akeno-uWS.js
This is for the fork of [µWebSockets.js](https://github.com/uNetworking/uWebSockets.js) used by [Akeno](https://github.com/the-lstv/akeno), used for building and including the µWebSockets server fork as a node module.
It moves a bunch of the JavaScript logic to C++ for slightly better performance (though it's not an enormous difference), eliminate calls to JavaScript where possible, and for the future low level cache implementation.

> [!NOTE] This fork is sadly not updated directly from upstream anymore, because the recent versions of uWebSockets include a lot of AI-generated code, which kind of goes against the standards of this library. I think it is irresponsible for a critical component of this scale to include AI code in production (considering they already had AI bugs & mistakes that made it to production), especially given that the library & it's developer doesn't have the best track record, and for that reason I want to ensure we aren't blindly including such code in this library.

## Warning
The state of this as of now is highly experimental and more of a proof-of-concept.

## Building
You can build on Linux and Mac OS, maybe on other UNIX-like systems. Windows may or may not work.

First, clone this repository:
```sh
git clone --branch master --single-branch --recursive https://github.com/the-lstv/akeno-uws.git
```
On Linux, this should be straightforward. Just ensure you have the following packages installed:
- Fedora (43): `@development-tools cmake git pkgconf-pkg-config libunwind-devel clang18 clang18-devel libstdc++-static`
- Ubuntu/Debian: `build-essential cmake clang-18 zlib1g-dev pkg-config libunwind-dev` (suggested by someone on GitHub; not tested by me)
<!-- 
If `lsquic` fails to compile, try updating it, as the version currently included in uSockets fails on Fedora:
```sh
cd uWebSockets/uSockets/lsquic
git pull origin master
cd ../../..
``` -->

To build, simply run:
```sh
make
```
Note that the makefile just bulids bulid.c and runs it. You can also do it directly:
```sh
gcc build.c
./a.out # <- Flags can go here.
```

Mac OS may be similar, I haven't tested it. uWebSockets supports it, but I've heard of some issues from time to time, so I can't guarantee it works 100%. It should be relatively similar to Linux, just like any other UNIX-like system.

Note that Windows is not supported by Akeno, so use at your own risk, I give no guarantee it runs well (if at all), even if I provide Windows binaries. I will try to help with issues, but it's the lowest priority.
I also do not provide any service reliability, security or safety guarantee for Windows (aka if it breaks and you happen to be using Windows, you're on your own, sorry).

## Licence
This repository contains code with different licences:
Code from µWebSockets.js is licensed under the Apache License 2.0, and some code originating from Akeno authors is licensed under the GNU General Public License v3.0 (since the Akeno project itself is licensed under GPLv3).
Thus the resulting binaries are also licensed under GPLv3.