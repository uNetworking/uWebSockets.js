<div align="center">
<img src="misc/logo.svg" height="180" />

*µWebSockets.js™ (it's "[micro](https://en.wikipedia.org/wiki/Micro-)") is simple, secure*<sup>[[1]](https://github.com/uNetworking/uWebSockets/tree/master/fuzzing)</sup> *& standards compliant web I/O for the most demanding*<sup>[[2]](https://github.com/uNetworking/uWebSockets/tree/master/benchmarks)</sup> *of JavaScript applications.*

• [TypeScript docs](https://unetworking.github.io/uWebSockets.js/generated/) • [Read more & user manual (C++ project)](https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md)

</div>

#### In a nutshell.
µWebSockets.js is the Google V8 bindings to [µWebSockets](https://github.com/uNetworking/uWebSockets), one of the most efficient web servers available for C++ programming<sup>[[2]](https://github.com/uNetworking/uWebSockets/tree/master/benchmarks)</sup>. Bypassing the entire I/O stack of Node.js allows for unprecedented efficiency in back-end JavaScript - what you build stands on nothing but the best of C and C++. Scales to millions of active WebSockets using half a GB of user space memory<sup>[[3]](https://medium.com/@alexhultman/millions-of-active-websockets-with-node-js-7dc575746a01)</sup>.

```
npm install uNetworking/uWebSockets.js#v15.8.0
```

, or any such tag (see [releases](https://github.com/uNetworking/uWebSockets.js/releases)).


#### Simple.
There are tons of [examples](examples) but here's the gist of it all:

```javascript
const uWS = require('uWebSockets.js');

/* Non-SSL is simply uWS.App() */
uWS.SSLApp({
  /* There are tons of SSL options */
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
}).ws('/*', {
  /* For brevity we skip the other events */
  message: (ws, message, isBinary) => {
    let ok = ws.send(message, isBinary);
  }
}).any('/*', (res, req) => {
  /* Let's deny all Http */
  res.end('Nothing to see here!');
}).listen(9001, (listenSocket) => {
  if (listenSocket) {
    console.log('Listening to port 9001');
  }
});
```

#### Pay what you want.
Commercially developed on a sponsored/consulting basis; BitMEX, Bitfinex and Coinbase are current or previous sponsors. Contact [me, the author](https://github.com/alexhultman) for support, feature development or consulting/contracting.

![](https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/2018.png)

*µWebSockets.js is intellectual property licensed Apache 2.0 with limitations on trademark use. Forks must be clearly labelled as such and must not be confused with the original.*
