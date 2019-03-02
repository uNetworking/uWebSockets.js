/* Minimal example that shuts down gracefully */

const uWS = require('../dist/uws.js');
const port = 9001;

/* We store the listen socket here, so that we can shut it down later */
let listenSocket;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/shutdown', (res, req) => {
  if (listenSocket) {
    res.end('Okay, shutting down now!');
    /* This function is provided directly by µSockets */
    uWS.us_listen_socket_close(listenSocket);
    listenSocket = null;
  } else {
    /* We just refuse if alrady shutting down */
    res.close();
  }
}).listen(port, (token) => {
  /* Save the listen socket for later shut down */
  listenSocket = token;
  /* Did we even manage to listen? */
  if (token) {
    console.log('Listening to port ' + port);

    /* Stop listening soon */
    setTimeout(() => {
      console.log('Shutting down now');
      uWS.us_listen_socket_close(listenSocket);
      listenSocket = null;
    }, 1000);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
