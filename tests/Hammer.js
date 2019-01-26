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

/* 0 to 1-less than max */
function getRandomInt(max) {
    return Math.floor(Math.random() * Math.floor(max));
}

function establishNewConnection() {
    const ws = new WebSocket('ws://localhost:' + port);

    ws.on('open', () => {
        /* Open more connections */
        if (++openedClientConnections < 1000) {
            establishNewConnection();
        } else {
            /* Stop listening */
            uWS.us_listen_socket_close(listenSocket);
        }

        console.log('Opened ' + openedClientConnections + ' client connections so far.');
        performRandomClientAction(ws);
    });

    /* It seems you can get messages after close?! */
    ws.on('message', (data) => {
        performRandomClientAction(ws);
    });

    ws.on('close', () => {
        console.log("client was closed");
        //performRandomClientAction(ws);
    });
}

/* Perform random websockets/ws action */
function performRandomClientAction(ws) {
    /* 0, 1 but never 2 */
    switch (getRandomInt(2)) {
        case 0: {
            ws.close();
            break;
        }
        case 1: {
            /* Length should be random from small to huge */
            try {
                ws.send('a test message');
            } catch (e) {
                /* Apparently websockets/ws can throw at any moment */

            }
            break;
        }
        case 2: {
            /* This should correspond to hard us_socket_close */
            ws.terminate();
            break;
        }
    }
}

/* Perform random uWebSockets.js action */
function performRandomServerAction(ws) {
    /* 0, 1 but never 2 */
    switch (getRandomInt(2)) {
        case 0: {
            ws.close();
            break;
        }
        case 1: {
            /* Length should be random from small to huge */
            ws.send('a test message', false);
            break;
        }
        case 2: {
            /* This should correspond to hard us_socket_close */
            ws.terminate();
            break;
        }
    }
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
    /* Doing a terminate here will be interesting */
    performRandomServerAction(ws);
  },
  message: (ws, message, isBinary) => {
    performRandomServerAction(ws);
  },
  drain: (ws) => {
    // todo: dont send over a certain backpressure
    //performRandomServerAction(ws);
  },
  close: (ws, code, message) => {
    try {
        performRandomServerAction(ws);
    } catch (e) {
        /* We expect to land here always, since socket is invalid */
        return;
    }
    /* We should never land here */
    uWS.print('ERROR: Did not throw in close!');
    process.exit(-1);
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