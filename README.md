<div align="center">
<img src="misc/logo.svg" height="180" />

*µWebSockets™ (it's "[micro](https://en.wikipedia.org/wiki/Micro-)") is simple, secure & standards compliant web I/O for the most demanding*<sup>[[1]](https://github.com/uNetworking/uWebSockets/tree/master/benchmarks)</sup> *of applications.*

• [Read more (redirects to C++ project)](https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md)

</div>


### For Node.js / NPM users
While not physically hosted by NPM, it's still possible to install using NPM tools like so ([npm docs](https://docs.npmjs.com/cli/install)):

```
npm install uNetworking/uWebSockets.js#v15.1.0
```

This project is a Google V8 (Node.js or not) web server built to be efficient and stable. It makes use of µWebSockets, the C++ project, and exposes its features to JavaScript developers. This includes mainly Http & WebSockets, SSL and non-SSL. Prominent users include bitfinex.com, they run ther trading APIs on this server.

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
#### Recursively clone, and enter, this repo:
```
git clone --recursive https://github.com/uNetworking/uWebSockets.js.git
cd uWebSockets.js
```
#### For Unix (Linux, macOS):
```
make
```
#### For Windows (in an x64 developer terminal):
```
nmake Windows
```
#### Test it out
```
node examples/HelloWorld.js
```
