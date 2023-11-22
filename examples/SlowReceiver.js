

const uWS = require('uWebSockets.js');
const port = 9001;

const wsc = require('ws');

const app = uWS.App().ws('/*', {
  /* Options */
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 10,
  maxBackpressure: 1024,
  closeOnBackpressureLimit: true, /* Enable or disable as needed */

  /* Handlers */
  open: (ws) => {
    console.log("Client connected");
    ws.subscribe('clock_feed');

    /* Disable closeOnBackpressureLimit and see this lie around 1024 */
    ws.clock = setInterval(() => {
      console.log("Connected socket has " + ws.getBufferedAmount() + " bytes buffered");
    }, 1000);
  },
  close: (ws, code, message) => {
    clearInterval(ws.clock);
    console.log("Client disconnected: " + code + ", " + message);
    /* The library guarantees proper unsubscription at close */
  }
}).listen(port, (listenSocket) => {
  if (listenSocket) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

/* Publish the time as fast as possible in a loop */
function sendTime() {
  app.publish("clock_feed", Date.now().toString());
  setImmediate(sendTime);
}
sendTime();

/* Make a connection to the server */
const ws = new wsc.WebSocket('ws://localhost:9001');
ws.on('message', function message(data) {
  console.log('received: %s', data);

  /* If we pause receive, we will become slow receiver */
  console.log("Will become slow receiver");
  ws._socket.pause();
});