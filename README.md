<div align="center">
<img src="https://github.com/uNetworking/v0.15/blob/master/misc/logo.png?raw=true" />

*µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") **for Node.js** is simple and efficient*<sup>[[1]](benchmarks)</sup> *messaging for the modern web.*

• [Read more](https://github.com/uNetworking/v0.15)

</div>

This repository tracks the development of Node.js bindings for µWebSockets v0.15.

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
