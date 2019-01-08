**µWebSockets.js** is a JavaScript platform, runtime and web server built on [µWebSockets](https://github.com/uNetworking/uWebSockets) v0.15 and V8.

There are two modes; compiled as a stand-alone JavaScript runtime or as a Node.js native addon.

For the most common Node.js systems are available precompiled binaries:

```
npm install uNetworking/uWebSockets.js#semver:0.0.1
```

 or any such version.

```javascript
/* The stand-alone runtime has uWS namespace already loaded. */
var uWS = uWS ? uWS : require('../dist/uws.js');

const world = 'Strings are slower than ArrayBuffer but who cares for demo purose!';
const port = 3000;

uWS.App().get('/hello', (res, req) => {
  res.end(world);
}).get('/*', (res, req) => {
  res.writeHeader('content-type', 'text/html; charset= utf-8').end(req.getHeader('user-agent') + ' är din user agent, biatch!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
```

### Benchmarks
Performance retention is up to 75% of native C++ [µWebSockets](https://github.com/uNetworking/uWebSockets) v0.15. That puts it some 20x as fast as Deno and even faster than most C++-only servers, all from within a JavaScript VM.
![](https://github.com/uNetworking/uWebSockets/blob/master/misc/bigshot_lineup.png)

### Build from source
Easiest is to compile yourself a Node.js native addon. The following works for Linux and macOS systems:
```
git clone --recursive https://github.com/uNetworking/uWebSockets.js.git
cd uWebSockets.js
make
node examples/HelloWorld.js
```
