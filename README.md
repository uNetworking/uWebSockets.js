<div align="center">
<img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/logo.svg" height="180" /><br>
<i>Simple, secure</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/fuzzing#fuzz-testing-of-various-parsers-and-mocked-examples">1</a></sup><i> & standards compliant</i><sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup><i> web server for the most demanding</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup><i> of applications.</i> <a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">Read more...</a>
<br><br>


<a href="https://github.com/uNetworking/uWebSockets.js/releases"><img src="https://img.shields.io/github/v/release/uNetworking/uWebSockets.js"></a> <a href="https://lgtm.com/projects/g/uNetworking/uWebSockets.js/context:cpp"><img alt="Language grade: C/C++" src="https://img.shields.io/lgtm/grade/cpp/g/uNetworking/uWebSockets.js.svg?logo=lgtm&logoWidth=18"/></a> <a href="https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:uwebsockets"><img src="https://oss-fuzz-build-logs.storage.googleapis.com/badges/uwebsockets.svg" /></a> <img src="https://img.shields.io/badge/downloads-65%20million-pink" />
</div>
<br><br>

### :zap: Simple performance
µWebSockets.js is an HTTP/WebSocket server for Node.js that runs **[8.5x that of Fastify](https://alexhultman.medium.com/serving-100k-requests-second-from-a-fanless-raspberry-pi-4-over-ethernet-fdd2c2e05a1e)** and at least **[10x that of Socket.IO](https://medium.com/swlh/100k-secure-websockets-with-raspberry-pi-4-1ba5d2127a23)**. It comes with both router and pub/sub support and is suited for extraordinary performance needs. Browse the [documentation](https://unetworking.github.io/uWebSockets.js/generated/) and see the [main repo](https://github.com/uNetworking/uWebSockets). There are tons of [examples](examples) but here's the gist of it all:

```javascript
/* Non-SSL is simply App() */
require('uWebSockets.js').SSLApp({

  /* There are more SSL options, cut for brevity */
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  
}).ws('/*', {

  /* There are many common helper features */
  idleTimeout: 30,
  maxBackpressure: 1024,
  maxPayloadLength: 512,
  compression: DEDICATED_COMPRESSOR_3KB,

  /* For brevity we skip the other events (upgrade, open, ping, pong, close) */
  message: (ws, message, isBinary) => {
    /* You can do app.publish('sensors/home/temperature', '22C') kind of pub/sub as well */
    
    /* Here we echo the message back, using compression if available */
    let ok = ws.send(message, isBinary, true);
  }
  
}).get('/*', (res, req) => {

  /* It does Http as well */
  res.writeStatus('200 OK').writeHeader('IsExample', 'Yes').end('Hello there!');
  
}).listen(9001, (listenSocket) => {

  if (listenSocket) {
    console.log('Listening to port 9001');
  }
  
});
```

### :muscle: Unfair advantage

Being written in native code directly targeting the Linux kernel makes it way faster than any JavaScript implementation.

![](misc/chart.png)

Low latencies are key to customer satisfaction and your competitive edge. Run low latency services at a lower cost.

![](misc/Manycast%20latency%20comparison%20%5Blower%20is%20better%5D.png)

### :crossed_swords: Battle proven
We've been fully standards compliant with a perfect Autobahn|Testsuite score since 2016<sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup>. µWebSockets powers many of the biggest crypto exchanges in the world, handling trade volumes of multiple billions of USD every day. If you trade crypto, chances are you do so via µWebSockets.

### :package: Easily installed
We *recommend, for simplicity* installing with `yarn add uWebSockets.js@uNetworking/uWebSockets.js#v20.3.0` or any such [release](https://github.com/uNetworking/uWebSockets.js/releases).

Being an open source project, you are of course perfectly free to choose other ways of installation as you might prefer.

### :briefcase: Commercially supported
<a href="https://github.com/uNetworking">uNetworking AB</a> is a Swedish consulting & contracting company dealing with anything related to µWebSockets; development, support and customer success.

Don't hesitate <a href="mailto:alexhultman@gmail.com">sending a mail</a> if you're building something large, in need of advice or having other business inquiries in mind. We'll figure out what's best for both parties and make sure you're not stepping into one of the many common pitfalls.

Special thanks to BitMEX, Bitfinex, Google, Coinbase, Bitwyre and deepstreamHub for allowing the project itself to thrive on GitHub since 2016 - this project would not be possible without these beautiful companies.

<img src="https://github.com/uNetworking/uWebSockets/raw/master/misc/2018.png" />

### :gear: Gear up
If performance is of utter importance, you don't necessarily have to use JavaScript/Node.js but could write apps in C++ using [µWebSockets](https://github.com/uNetworking/uWebSockets) directly. It works exactly the same way, and will offer the best performance for those highly demanding applications where scripting won't do.

### :handshake: Permissively licensed
Intellectual property, all rights reserved.

Where such explicit notice is given, source code is licensed Apache License 2.0 which is a permissive OSI-approved license with very few limitations. Modified "forks" should be of nothing but licensed source code, and be made available under another product name. If you're uncertain about any of this, please ask before assuming.
