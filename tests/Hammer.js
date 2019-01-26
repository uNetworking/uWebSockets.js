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

const maxBackpressure = 50 * 1024 * 1024;
const maxPayloadSize = 5 * 1024 * 1024;
const largeBuffer = new ArrayBuffer(maxPayloadSize);

function printStatistics() {
    console.log('Opened clients ' + openedClientConnections);
    console.log('Closed clients ' + closedClientConnections + '\n');
}

/* 0 to 1-less than max */
function getRandomInt(max) {
    return Math.floor(Math.random() * Math.floor(max));
}

/* This gets called every time we either open a new connection,
 * or it immediately land in websockets/ws error handler */
function accountForConnection() {
    /* Open more connections */
    if (++openedClientConnections < /*100*/ 1000) {
        establishNewConnection();
    } else {
        /* Stop listening */
        uWS.us_listen_socket_close(listenSocket);
    }
}

function establishNewConnection() {
    const ws = new WebSocket('ws://localhost:' + port);

    ws.on('open', () => {
        /* Mark this socket as opened */
        ws._opened = true;
        accountForConnection();

        printStatistics();
        performRandomClientAction(ws);
    });

    /* It seems you can get messages after close?! */
    ws.on('message', (data) => {
        performRandomClientAction(ws);
    });

    /* Close can be called more times than open, websockets/ws is really messy! */
    ws.on('close', () => {
        /* Check if this websocket really was opened first */
        if (!ws._opened) {
            accountForConnection();
        }

        closedClientConnections++;
        printStatistics();
        /* I guess there is no need for us to call anything
         * here, websockets/ws will not send anything to us anyways */
        //performRandomClientAction(ws);
    });

    ws.on('error', () => {
        /* We simply ignore errors. websockets/ws will call our close handler
         * on errors, potentially before even calling the open handler */
    });
}

/* Perform random websockets/ws action */
function performRandomClientAction(ws) {
    /* 0, 1, 2 but never 3 */
    let action = getRandomInt(3);

    /* Sending a message should have higher probability */
    if (getRandomInt(100) < 80) {
        action = 1;
    }

    switch (action) {
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
            ws.terminate();
            break;
        }
    }
}

/* Perform random uWebSockets.js action */
function performRandomServerAction(ws, uniform) {
    /* 0, 1, 2 but never 3 */
    let action = getRandomInt(3);

    /* Sending a message should have higher probability,
     * except for when we are draining. */
    if (!uniform) {
        if (getRandomInt(100) < 80) {
            action = 1;
        }
    }

    switch (action) {
        case 0: {
            ws.close();
            break;
        }
        case 1: {
            /* If we have very high backpressure we skip sending more */
            if (ws.getBufferedAmount() > maxBackpressure) {
                /* Do something else instead */
                performRandomServerAction(ws);
            } else {
                /* Length should be random from small to huge, ArrayBuffer.slice is a copy (bad but we don't care I guess) */
                ws.send(largeBuffer.slice(0, getRandomInt(maxPayloadSize + 1)));
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

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).ws('/*', {
  compression: 0,
  /* We intentionally accept less than what might be sent so to test force closing */
  maxPayloadLength: maxPayloadSize / 2,
  idleTimeout: 100,

  open: (ws, req) => {
    /* Doing a terminate here will be interesting */
    performRandomServerAction(ws, false);
  },
  message: (ws, message, isBinary) => {
    performRandomServerAction(ws, false);
  },
  drain: (ws) => {
    /* We only perform actions while draining rarely (5%), and we do it uniformly.
     * There's no real NEED to do anything here, as it will eventually result in
     * message event for client. */
    if (getRandomInt(100) < 5) {
        performRandomServerAction(ws, true);
    }
  },
  close: (ws, code, message) => {
    /* TODO: We SHOULD TECHNICALLY allow sending here.
     * It should not throw, but just silently ignore it
     * as it makes no sense to send on a closed socket
     * while still being technically valid */
    try {
        performRandomServerAction(ws, false);
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
    uWS.print('Exiting now');
    app.forcefully_free();
});