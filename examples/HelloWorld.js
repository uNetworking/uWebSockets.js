/* The stand-alone runtime has uWS namespace already loaded. */
var uWS = uWS ? uWS : require('../dist/uws.js');

const port = 3000;

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
  message: (ws, message) => {
    ws.send(message);
  }
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

/* This is not true for stand-alone */
console.log('Timers will not work until us_loop_integrate is done');
