/* A few users have reported having issues understanding backpressure and how to deal with it.
 *
 * Backpressure is the buildup of unacknowledged data; you can't just call ws.send without checking for backpressure.
 * Data doesn't just, poof, immediately jump from your server to the receiver - the receiver has to actually... receive it.
 * That happens with ACKs, controlling the transmission window.
 *
 * See https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/bufferedAmount
 *
 * Backpressure applies to all streams, files, sockets, queues, and so on. If you're building
 * web services without taking backpressure into account you're not developing proper solutions - you're dicking around.
 *
 * Any slow receiver can DOS your whole server if you're not taking backpressure into account.
 *
 * The following is a (ridiculous) example of how data can be pushed according to backpressure.
 * Do not take this as a way to actually write code, this is horrible, but it shows the concept clearly.
 *
 */

/* Number between thumb and index finger */
const backpressure = 1024;

/* Used for statistics */
let messages = 0;
let messageNumber = 0;

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
  /* We need a slightly higher timeout for this crazy example */
  idleTimeout: 60,
  /* Handlers */
  open: (ws) => {
    console.log('A WebSocket connected!');
    /* We begin our example by sending until we have backpressure */
    while (ws.getBufferedAmount() < backpressure) {
        ws.send("This is a message, let's call it " + messageNumber);
        messageNumber++;
        messages++;
    }
  },
  drain: (ws) => {
    /* Continue sending when we have drained (some) */
    while (ws.getBufferedAmount() < backpressure) {
      ws.send("This is a message, let's call it " + messageNumber);
      messageNumber++;
      messages++;
    }
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

/* Start a timer to check how fast we end up sending.
 * Not a benchmark, just statistics. */
setInterval(() => {
    console.log("Sent " + messages + " messages last second");
    messages = 0;
}, 1000);
