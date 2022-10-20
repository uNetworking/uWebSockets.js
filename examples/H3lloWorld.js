/* Minimal HTTP/3 example */

const uWS = require('../dist/uws.js');
const port = 9001;

/* ./quiche-client --no-verify https://localhost:9001 */

/* The only difference here is that we use uWS.H3App rather than uWS.App or uWS.SSLApp.
 * And of course, there are no WebSockets in HTTP/3 only WebTransport (coming) */

const app = uWS.H3App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/*', (res, req) => {
  res.end('H3llo World!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
