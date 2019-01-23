/* Test server for autobahn, run with ASAN. /exit route shuts down. */

const uWS = require('../dist/uws.js');
let listenSocket, listenSocketSSL;

/* todo: A good idea is to test compression 1 for non-SSL and 2 for SSL */
/* or really just test more than twice */

/* Shared, among SSL and non-SSL, behavior of WebSockets */
const wsBehavior = {
  /* Options */
  compression: 2,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 60,
  /* Handlers */
  open: (ws, req) => {
    console.log('A WebSocket connected via URL: ' + req.getUrl() + '!');
  },
  message: (ws, message, isBinary) => {
    ws.send(message, isBinary);
  },
  drain: (ws) => {
    console.log('WebSocket backpressure: ' + ws.getBufferedAmount());
  },
  close: (ws, code, message) => {
    console.log('WebSocket closed');
  }
};

/* The SSL test server is on port 9002 */
const sslApp = uWS.SSLApp({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', wsBehavior).any('/exit', (res, req) => {
  /* Stop listening on both SSL and non-SSL */
  if (listenSocket) {
    uWS.us_listen_socket_close(listenSocket);
    uWS.us_listen_socket_close(listenSocketSSL);
    listenSocket = null;
  }

  /* Close this connection */
  res.close();
}).listen(9002, (token) => {
  if (token) {
    listenSocketSSL = token;
    console.log('SSL test server is up...');
  }
});

/* The non-SSL test server is on port 9001 */
const app = uWS.App().ws('/*', wsBehavior).any('/exit', (res, req) => {
  /* Stop listening on both SSL and non-SSL */
  if (listenSocket) {
    uWS.us_listen_socket_close(listenSocket);
    uWS.us_listen_socket_close(listenSocketSSL);
    listenSocket = null;
  }

  /* Close this connection */
  res.close();
}).listen(9001, (token) => {
  if (token) {
    listenSocket = token;
    console.log('Non-SSL test server is up...');
  }
});

/* This is required to check for memory leaks */
process.on('beforeExit', () => {
  app.forcefully_free();
  sslApp.forcefully_free();
});
