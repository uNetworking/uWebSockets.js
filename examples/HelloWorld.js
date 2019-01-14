/* The stand-alone runtime has uWS namespace already loaded. */
var uWS = uWS ? uWS : require('../dist/uws.js');

const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: '/home/alexhultman/key.pem',
  cert_file_name: '/home/alexhultman/cert.pem',
  passphrase: '1234'
}).get('/static/:param', (res, req) => {
  res.end('Hello, your url is ' + req.getUrl() + ". The param1 is " + req.getParameter(0));
}).get('/hello', (res, req) => {
  res.end('Hej, du är på url: ' + req.getUrl());
}).ws('/*', {
  compression: 0,
  maxPayloadLength: 16 * 1024 * 1024,
  open: (ws, req) => {
    console.log(ws);
  },
  message: (ws, message, isBinary) => {
    ws.send(message, isBinary);

    console.log('BufferedAmount is ' + ws.getBufferedAmount());

    ws.close();
  },
  drain: (ws) => {
    console.log('WebSocket drained');
  },
  close: (ws, code, message) => {
    console.log('WebSocket closed');
  }
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
