/* Simple pub/sub example (WIP) */

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
  // would be nice to have maxBackpressure to automatically close slow receivers
  /* Handlers */
  open: (ws, req) => {
    /* Let all new sockets subscribe to chat topic */
    ws.subscribe('chat');
  },
  message: (ws, message, isBinary) => {
    console.log('Got WebSocket message!');
    /* If we get a message from any socket, we publish it to the chat topic */
    ws.publish('chat', message);
  },
  drain: (ws) => {

  },
  close: (ws, code, message) => {
    // here we need to properly unsubscribe
    // ws.unsubscribe('*');
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
