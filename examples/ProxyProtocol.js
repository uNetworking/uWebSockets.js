/* Minimal SSL/non-SSL example using PROXY Protocol v2 */

const uWS = require('../dist/uws.js');
const port = 3000;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/*', (res, req) => {
  /* Respond with their addresses */
  res.cork(() => {
    res.write('<html><h1>');
    res.write('Your proxied IP is: ' + Buffer.from(res.getProxiedRemoteAddressAsText()).toString());
    res.write('</h1><h1>');
    res.write('Your IP as seen by the origin server is: ' + Buffer.from(res.getRemoteAddressAsText()).toString());
    res.end('</h1></html>');
  });
}).listen(port, (listenSocket) => {
  if (listenSocket) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
