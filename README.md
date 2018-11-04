Loopery is Google V8 tied to µWebSockets for building high performance JavaScript backend services.

```javascript
const uWS = require('...lalala');

const app = uWS.SSLApp({
  cert: 'my_cert.pem',
  key: 'my_key.pem'
}).get('/whatsmy/useragent', (res, req) => {
  res.end('Hello ' + req.getHeader('user-agent'));
}).get('/*', (res, req) => {
  res.end('Hello otherwise!');
}).listen(3000, 0, (token) => {
  console.log('Listening to port 3000');
});
```

### Benchmarks
Performance retention is up to 70% of native C++ µWebSockets v0.15.
![](benchmarks.png)

### Kick-start
The following works for Linux and macOS systems:
```
git clone --recursive https://github.com/uNetworking/uWebSockets-node.git
cd uWebSockets-node
make
node examples/HelloWorld.js
```
