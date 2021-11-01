### :construction: This is a fork!

This is a fork of the original [uWebSockets.js](https://github.com/uNetworking/uWebSockets.js) by @alexhultman. This fork adds unofficial support for Electron builds (Electron version 8, 9, 10, 11, and 12) by changing the build pipeline to use `node-gyp` instead of a custom C++ build script which allows us to use [electron-rebuild](https://github.com/electron/electron-rebuild). This repo is mainly for internal use to support Truffle Suite's [Ganache UI](https://github.com/trufflesuite/ganache) Electron application.

**NOTE**: These binaries **do not** support SSL or Compression. They were not necessary for our uses, and we had issues getting those to compile with the Electron headers.

### :handshake: Permissively licensed by uNetworking AB
Intellectual property and all rights reserved by [uNetworking](https://github.com/uNetworking/). The [license](./LICENSE) in this repository is the one kept from the [original repository](https://github.com/uNetworking/uWebSockets.js).

Where such explicit notice is given, source code is licensed Apache License 2.0 which is a permissive OSI-approved license with very few limitations. Modified "forks" should be of nothing but licensed source code, and be made available under another product name. If you're uncertain about any of this, please ask before assuming.
