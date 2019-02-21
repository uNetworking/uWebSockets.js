/* Test servers for autobahn, run with ASAN. /exit route shuts down everything */
const uWS = require('../dist/uws.js');

/* Keep track of all apps */
let apps = [];
let closing = false;

function listenWithSettings(settings) {
  /* These are our shared SSL options */
  let sslOptions = {
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
  };

  /* Create the app */
  let app = settings.ssl ? uWS.SSLApp(sslOptions) : uWS.App(sslOptions);

  /* Attach our behavior from settings */
  app.ws('/*', {
    compression: settings.compression,
    maxPayloadLength: 16 * 1024 * 1024,
    idleTimeout: 60,
    message: (ws, message, isBinary) => {
      ws.send(message, isBinary, true);
    }
  }).any('/exit', (res, req) => {
    /* Shut down everything on this route */
    if (!closing) {
      apps.forEach((a) => {
        uWS.us_listen_socket_close(a.listenSocket);
      });
      closing = true;
    }

    /* Close this connection */
    res.close();
  }).listen(settings.port, (listenSocket) => {
    if (listenSocket) {
      /* Add this app with its listenSocket */
      apps.push({
        app: app,
        listenSocket: listenSocket
      });
      console.log('Up and running: ' + JSON.stringify(settings));
    } else {
      /* Failure here */
      console.log('Failed to listen, closing everything now');
      process.exit(0);
    }
  });
}

/* non-SSL, non-compression */
listenWithSettings({
  port: 9001,
  ssl: false,
  compression: uWS.DISABLED
});

/* SSL, shared compressor */
listenWithSettings({
  port: 9002,
  ssl: true,
  compression: uWS.SHARED_COMPRESSOR
});

/* non-SSL, dedicated compressor */
listenWithSettings({
  port: 9003,
  ssl: false,
  compression: uWS.DEDICATED_COMPRESSOR
});

/* This is required to check for memory leaks */
process.on('exit', () => {
  apps.forEach((a) => {
    a.app.forcefully_free();
  });
});
