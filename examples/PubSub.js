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

  /* Setting 1: merge messages in one, or keep them as separate WebSocket frames - mergePublishedMessages */
  /* Setting 2: compression on/off - cannot have dedicated compressor for pubsub yet */
  /* Setting 3: maxBackpressure - when we want to automatically terminate a slow receiver */
  /* Setting 4: send to all including us, or not? That's not a setting really just use ws.publish or global uWS.publish */

  /* Handlers */
  open: (ws, req) => {
    /* Let this client listen to all sensor topics */
    ws.subscribe('home/sensors/#');
  },
  message: (ws, message, isBinary) => {
    /* Parse this message according to some application
     * protocol such as JSON [action, topic, message] */

    /* Let's pretend this was a temperature message
     * [pub, home/sensors/temperature, message] */
    ws.publish('home/sensors/temperature', message);

    /* Let's also pretend this was a light message
     * [pub, home/sensors/light, message] */
    ws.publish('home/sensors/light', message);

    /* If you have good imagination you can also
     * pretend some message was a subscription
     * like so: [sub, /home/sensors/humidity].
     * I hope you get how any application protocol
     * can be implemented with these helpers. */
  },
  drain: (ws) => {

  },
  close: (ws, code, message) => {
    /* The library guarantees proper unsubscription at close */
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
