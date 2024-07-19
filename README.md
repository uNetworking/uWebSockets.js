<div align="center">
<img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/logo.svg" height="180" /><br>
<i>Simple, secure</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/fuzzing#fuzz-testing-of-various-parsers-and-mocked-examples">1</a></sup><i> & standards compliant</i><sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup><i> web server for the most demanding</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup><i> of applications.</i> <a href="https://github.com/uNetworking/uWebSockets#readme">Read more...</a>
<br><br>

<a href="https://github.com/uNetworking/uWebSockets.js/releases"><img src="https://img.shields.io/github/v/release/uNetworking/uWebSockets.js"></a> <a href="https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:uwebsockets"><img src="https://oss-fuzz-build-logs.storage.googleapis.com/badges/uwebsockets.svg" /></a> <img src="https://img.shields.io/badge/established-in%202016-green" /> <a href="https://twitter.com/uNetworkingAB"><img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/follow.png" height="20"/></a>
</div>
<br>

### :zap: Simple performance
µWebSockets.js is a standards compliant web server written in 10,000 lines of C++. It is exposed to Node.js as a simple-to-use, native V8 addon and performs at least **[10x that of Socket.IO](https://medium.com/swlh/100k-secure-websockets-with-raspberry-pi-4-1ba5d2127a23)**, [8.5x that of Fastify](https://alexhultman.medium.com/serving-100k-requests-second-from-a-fanless-raspberry-pi-4-over-ethernet-fdd2c2e05a1e). It makes up the **[core components of Bun](https://twitter.com/uNetworkingAB/status/1810380862556397887)** and is the **[fastest standards compliant web server](https://x.com/uNetworkingAB/status/1812914159295869075)** in the TechEmpower (**[not endorsed](https://x.com/uNetworkingAB/status/1811425564764610926)**) benchmarks.

We aren't in the NPM registry but you can easily install it with the NPM client:
* `npm install uNetworking/uWebSockets.js#v20.44.0`
* Browse the [documentation](https://unetworking.github.io/uWebSockets.js/generated/functions/App.html) and see the [main repo](https://github.com/uNetworking/uWebSockets). There are tons of [examples](examples) but here's the gist of it all:

### :dart: In essence

![Untitled](https://github.com/user-attachments/assets/76647341-1de9-4646-abde-54a8f6a8c1e2)

### :handshake: Permissively licensed
Intellectual property, all rights reserved.

Where such explicit notice is given, source code is licensed Apache License 2.0 which is a permissive OSI-approved license with very few limitations. Modified "forks" should be of nothing but licensed source code, and be made available under another product name. If you're uncertain about any of this, please ask before assuming.
