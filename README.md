<div align="center">
<img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/logo.svg" height="180" /><br>
<i>Simple, secure</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/fuzzing#fuzz-testing-of-various-parsers-and-mocked-examples">[1]</a></sup><i> & standards compliant</i><sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">[2]</a></sup><i> web server for the most demanding</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">[3]</a></sup><i> of applications.</i> <a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">Read more...</a>
<br><br>


<a href="https://github.com/uNetworking/uWebSockets.js/releases"><img src="https://img.shields.io/github/v/release/uNetworking/uWebSockets.js"></a> <a href="https://lgtm.com/projects/g/uNetworking/uWebSockets.js/context:cpp"><img alt="Language grade: C/C++" src="https://img.shields.io/lgtm/grade/cpp/g/uNetworking/uWebSockets.js.svg?logo=lgtm&logoWidth=18"/></a> <img src="https://img.shields.io/badge/downloads-50,000,000+-green" />

</div>
<br><br>

### :bulb: In a nutshell

Think of it as a complete replacement to both Express.js and Socket.IO, written entirely in C/C++ for maximum performance and reliability. Browse the [documentation](https://unetworking.github.io/uWebSockets.js/generated/). There are tons of [examples](examples) but here's the gist of it all:

```javascript
/* Non-SSL is simply App() */
require('uWebSockets.js').SSLApp({

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

### :rocket: Ready all thrusters

Install with `npm install uNetworking/uWebSockets.js#v18.1.0` or any such [release](https://github.com/uNetworking/uWebSockets.js/releases). No compiler needed.

![](misc/features_strip.png)

Real-world tests over TLS 1.3 and Ethernet puts us **5x** as efficient as Socket.IO, **2x** as efficient as websockets/ws.

### :dollar: Pay what you want
Commercially developed on a sponsored/consulting basis; BitMEX, Bitfinex and Coinbase are current or previous sponsors. Contact [me, the author](https://github.com/alexhultman) for support, feature development or consulting/contracting.

![](https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/2018.png)

### :mortar_board: Know thy legal matters

*µWebSockets.js is intellectual property licensed Apache 2.0 with limitations on trademark use. Forks must be clearly labelled as such and must not be confused with the original.*
