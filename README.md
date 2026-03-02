<p align="center">
  <img src="https://github.com/user-attachments/assets/a742cb99-7423-4be7-8e2c-f12b8324fae7" width="900"/>
</p>

# Akeno-uWS.js
This repository is for the fork of µWebSockets.js used by [Akeno](https://github.com/the-lstv/akeno), used for building and including the µWebSockets as a module.
It moves a bunch of the JavaScript logic to C++ for slightly better performance (though it's not an enormous difference), eliminate calls to JavaScript where possible, and for the future low level cache implementation.

#### This is *not* the main repository for Akeno - please [go there instead](https://github.com/the-lstv/akeno).

## Warning
The state of this as of now is highly experimental and more of a proof-of-concept.

## Usage
This is designed to be used by Akeno. If you place them in this structure:
```sh
/akeno        # Akeno installation
/akeno-uws    # This repository
```
Akeno (1.6.9-beta and up) should automatically discover this repository and try to use it's build.
In the future this will be included directly in Akeno, but that is once it is not experimental.

## Building
You can build on Linux and Mac OS, maybe on other UNIX-like systems. Windows is not supported, but may work.

First, clone this repository:
```sh
git clone --branch master --single-branch --recursive https://github.com/the-lstv/akeno-uws.git
```
On Linux, this should be straightforward. Just ensure you have the following packages installed:
- Fedora (43): `@development-tools cmake git pkgconf-pkg-config libunwind-devel clang18 clang18-devel libstdc++-static`
- Ubuntu/Debian: `build-essential cmake clang-18 zlib1g-dev pkg-config libunwind-dev` (suggested by someone on GitHub)

If `lsquic` fails to compile, try updating it, as the version currently included in uSockets fails on Fedora:
```sh
cd uWebSockets/uSockets/lsquic
git pull origin master
cd ../../..
```

To build, simply run:
```sh
make
```
Note that the makefile just bulids bulid.c and runs it. You can also do it directly:
```sh
gcc build.c
./a.out # <- Flags can go here.
```

Mac OS may be similar, I haven't tested it. uWebSockets supports it, but I've heard of some issues from time to time, so I can't guarantee it works. It should be relatively similar to Linux, just like any other UNIX-like system.

If you really *must* compile on Windows, a recommended way to do that is to not bulid on Windows at all, but use WSL or Linux (via a VM or simillar) and cross-compile with the `--cross-windows` flag, which should output Windows binaries. Compiling on Windows itself is going to be painful and I don't recommend it. If you can't use WSL, I recommend some kind of service (Github, run.lstv.space, etc.), only compile on Windows as a last resort.
Note that Windows is not officially supported by Akeno, so use at your own risk, I give no guarantee it runs well (if at all) on Windows. I will try to help with issues, but it's the lowest priority.
I also do not provide any service reliability, security or safety guarantee for Windows.

Example for cross-compiling on Linux to Windows:
```sh
gcc build.c
./a.out --cross-windows --no-http3
```

## Licence
This repository contains code with different licences:
Code from µWebSockets.js is licensed under the Apache License 2.0, and some code originating from Akeno authors is licensed under the GNU General Public License v3.0 (since the Akeno project itself is licensed under GPLv3).
Thus the resulting binaries are also licensed under GPLv3.

> [!NOTE]
> All issues/PRs should go to the [main Akeno repository](https://github.com/the-lstv/akeno).