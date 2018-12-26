/* The stand-alone runtime has uWS namespace already loaded. */
var uWS = uWS ? uWS : require('../dist/uws.js');

const port = 9001;

// nice milestone to pass autobahn on both ssl and non-ssl with an without compression

const app = uWS./*SSL*/App({
  key_file_name: '/home/alexhultman/key.pem',
  cert_file_name: '/home/alexhultman/cert.pem',
  passphrase: '1234'
}).get('/hello', (res, req) => {
  res.end('Hejdå!');
}).ws('/*', {
  open: (ws, req) => {
    console.log(ws);
  },
  message: (ws, message, opCode) => {
    ws.send(message, opCode); // need op to pass autobahn (guess binary on off is enough?)
  }
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
