<div align="center">
<img src="misc/logo.svg" height="180" />

*µWebSockets™ (it's "[micro](https://en.wikipedia.org/wiki/Micro-)") is simple, secure & standards compliant web I/O for the most demanding*<sup>[[1]](benchmarks)</sup> *of applications.*

• [Read more](misc/READMORE.md)

</div>


### NPM install
While not physically hosted by NPM, it's still possible to install using NPM tools like so (or any such version):

```
npm install uNetworking/uWebSockets.js#semver:0.0.1
```

### Presenting; faster I/O for Node.js
Every other week a new "web framework" pops up for Node.js. Fastify, Restana, Aero, Micro and so on. Most aim to improve I/O performance, but fall flat. You can't fix an inherent design flaw of an entire stack just by slapping a few lines of JavaScript on top.

µWS.js is a 100% C/C++ web platform completely bypassing the entire Node.js I/O stack. JavaScript-exposed functions and objects are entirely backed by native code all the way down to the OS kernel. This makes it possible to reach unprecedented I/O performance from within Node.js (or any stand-alone V8 build). Scroll down for benchmarks.

##### In a nutshell
```javascript
const uWS = require('uWebSockets.js');
const port = 3000;

uWS.SSLApp({
  /* cert, key and such specified here, or use uWS.App */
}).get('/whatsmyuseragent', (res, req) => {
  res.writeHeader(
    'content-type', 'text/html; charset= utf-8'
  ).end(
    'Your user agent is: ' + req.getHeader('user-agent')
  );
}).get('/*', (res, req) => {
  res.end('Wildcard route');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
```

### Streams
Proper streaming of huge data is supported over Http/Https and demonstrated with examples. Here's a shot of me watching real-time streamed HD video from Node.js while simultaneously scoring a 115k req/sec with wrk. For my computer, that's about 5x that of vanilla Node.js (without any HD video streaming/playing).

![](misc/streaming.png)

### Benchmarks
Performance retention is about 65% that of the native C++ [µWebSockets](https://github.com/uNetworking/uWebSockets) v0.15. That makes it some 20x as fast as Deno and even faster than most C++-only servers, all from within a JavaScript VM.

Http | WebSockets
--- | ---
![](https://github.com/uNetworking/uWebSockets/blob/master/misc/bigshot_lineup.png) | ![](https://github.com/uNetworking/uWebSockets/blob/master/misc/websocket_lineup.png)

### Build from source
Easiest is to compile yourself a Node.js native addon. The following works for Linux and macOS systems:
```
git clone --recursive https://github.com/uNetworking/uWebSockets.js.git
cd uWebSockets.js
make
node examples/HelloWorld.js
```
