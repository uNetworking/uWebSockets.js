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

function getRandomInt(max) {
    return Math.floor(Math.random() * Math.floor(max));
}

/* Perform random websockets/ws action */
function performRandomClientAction(ws) {
    uWS.print('Random client action');
    ws.send('test message from client');
    uWS.print('Random client action, done');
}

/* Perform random uWebSockets.js action */

function establishNewConnection() {
    const ws = new WebSocket('ws://localhost:' + port);

    ws.on('open', () => {
        /* Open more connections */
        if (++openedClientConnections < 1) {
            establishNewConnection();
        } else {
            /* Stop listening */
            uWS.us_listen_socket_close(listenSocket);
        }

        console.log('Opened ' + openedClientConnections + ' client connections so far.');
        performRandomClientAction(ws);
    });

    ws.on('message', (data) => {
        console.log('client got message: ' + data);
        performRandomClientAction(ws);
    });

    ws.on('close', () => {
        console.log("client was closed");
        //performRandomClientAction(ws);
    });
}


function performRandomServerAction(ws) {
    //uWS.print('Random server action');
    switch (getRandomInt(1)) {
        case 0: {
            ws.close();
            break;
        }
        case 1: {
            ws.send('a test message', false);
            break;
        }
    }
    //uWS.print('Done with random server action');
}

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', {
  compression: 0,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 10,

  open: (ws, req) => {
    uWS.print('Server open event');
    performRandomServerAction(ws);
    uWS.print('Server open event, returning');
  },
  message: (ws, message, isBinary) => {
    console.log('server got message: ' + message.byteLength);
    performRandomServerAction(ws);
  },
  drain: (ws) => {
    // todo: dont send over a certain backpressure
    //performRandomServerAction(ws);
  },
  close: (ws, code, message) => {
    console.log('server was closed');
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