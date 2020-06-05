/* A quite detailed WebSockets example */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', {
  /* Options */
  compression: 0,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 10,
  /* Handlers */
  upgrade: (res, req, context) => {
    console.log('An Http connection wants to become WebSocket, URL: ' + req.getUrl() + '!');

    res.upgrade({url: req.getUrl()}, req.getHeader('sec-websocket-key'),
      req.getHeader('sec-websocket-protocol'),
      req.getHeader('sec-websocket-extensions'),
      context);
  },
  open: (ws) => {
    console.log('A WebSocket connected with URL: ' + ws.userData.url);
  },
  message: (ws, message, isBinary) => {
    /* Ok is false if backpressure was built up, wait for drain */
    let ok = ws.send(message, isBinary);
  },
  drain: (ws) => {
    console.log('WebSocket backpressure: ' + ws.getBufferedAmount());
  },
  close: (ws, code, message) => {
    console.log('WebSocket closed');
  }
}).any('/*', (res, req) => {
  res.end('Nothing to see here!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
