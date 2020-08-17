/* Minimal SSL example using ServerNameIndication */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS.SSLApp({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).missingServerName((hostname) => {

  /* Note: You don't have to use this callback but can pre-load
   * relevant server names up front. This callback is not "async",
   * you either add the server name HERE IMMEDIATELY, or the hangshake
   * will continue with default certificate (which will most likely fail) */

  console.log("Hello! We are missing server name <" + hostname + ">");

  /* We simply assume it is localhost, so adding it here */
  app.addServerName("localhost", {
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
  });

}).get('/*', (res, req) => {
  res.end('Hello World!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
