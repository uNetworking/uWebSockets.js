/* Welcome to the so called hammer test, or if you want, moron test.
 * Here we test hammering the library in all kinds of crazy ways.
 * Establish connections, have them perform random operations so to
 * randomly explore the entire state space while performing random
 * operations to test strange edge cases. */

/* Run with AddressSanitizer enabled for best effect. */

/* We use websockets/ws as client counterpart */
const WebSocket = require('ws');

const uWS = require('../dist/uws.js');
const port = 9001;

let openedClientConnections = 0;
let closedClientConnections = 0;

let listenSocket;

/* Perform random websockets/ws action */
function performRandomClientAction(ws) {
    console.log("performing random ws action");

    ws.send('test message from client');
}

/* Perform random uWebSockets.js action */

function establishNewConnection() {
    const ws = new WebSocket('ws://localhost:' + port);

    ws.on('open', () => {
        /* Open more connections */
        if (++openedClientConnections < 10) {
            establishNewConnection();
        } else {
            /* Stop listening */
            uWS.us_listen_socket_close(listenSocket);
        }

        console.log('Opened ' + openedClientConnections + ' client connections so far.');
        performRandomClientAction(ws);
    });

    ws.on('message', (data) => {
        performRandomClientAction(ws);
    });

    ws.on('close', () => {
        performRandomClientAction(ws);
    });
}


function performRandomServerAction(ws) {
    console.log("performing random server action");

    ws.send('a test message');
}

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', {
  /*compression: 0,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 10,*/

  open: (ws, req) => {
    performRandomServerAction(ws);
  },
  message: (ws, message, isBinary) => {
    performRandomServerAction(ws);
  },
  drain: (ws) => {
    //performRandomServerAction(ws);
  },
  close: (ws, code, message) => {
    //performRandomServerAction(ws);
  }
}).any('/*', (res, req) => {
  res.end('Nothing to see here!');
}).listen(port, (token) => {
  if (token) {
    console.log('Hammering on port ' + port);

    /* Establish first connection */
    establishNewConnection();

    /* Use this to stop listening when done */
    listenSocket = token;
  }
});

/* Yes we do this crazy thing here */
process.on('beforeExit', () => {
    app.forcefully_free();
});