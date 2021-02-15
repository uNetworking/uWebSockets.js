/* A quite detailed WebSockets upgrade example "async" */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', {
  /* Options */
  compression: uWS.SHARED_COMPRESSOR,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 10,
  /* Handlers */
  upgrade: (res, req, context) => {
    console.log('An Http connection wants to become WebSocket, URL: ' + req.getUrl() + '!');

    /* Keep track of abortions */
    const upgradeAborted = {aborted: false};

    /* You MUST copy data out of req here, as req is only valid within this immediate callback */
    const url = req.getUrl();
    const secWebSocketKey = req.getHeader('sec-websocket-key');
    const secWebSocketProtocol = req.getHeader('sec-websocket-protocol');
    const secWebSocketExtensions = req.getHeader('sec-websocket-extensions');

    /* Simulate doing "async" work before upgrading */
    setTimeout(() => {
      console.log("We are now done with our async task, let's upgrade the WebSocket!");

      if (upgradeAborted.aborted) {
        console.log("Ouch! Client disconnected before we could upgrade it!");
        /* You must not upgrade now */
        return;
      }

      /* This immediately calls open handler, you must not use res after this call */
      res.upgrade({
        url: url
      },
      /* Use our copies here */
      secWebSocketKey,
      secWebSocketProtocol,
      secWebSocketExtensions,
      context);
    }, 1000);

    /* You MUST register an abort handler to know if the upgrade was aborted by peer */
    res.onAborted(() => {
      /* We can simply signal that we were aborted */
      upgradeAborted.aborted = true;
    });
  },
  open: (ws) => {
    console.log('A WebSocket connected with URL: ' + ws.url);
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
