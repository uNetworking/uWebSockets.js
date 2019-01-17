/* Simple demonstration of some router features */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: '/home/alexhultman/key.pem',
  cert_file_name: '/home/alexhultman/cert.pem',
  passphrase: '1234'
}).get('/user/agent', (res, req) => {
  /* Read headers */
  res.end('Your user agent is: ' + req.getHeader('user-agent') + ' thank you, come again!');
}).get('/static/yes', (res, req) => {
  /* Static match */
  res.end('This is very static');
}).get('/candy/:kind', (res, req) => {
  /* Parameters */
  res.end('So you want candy? Have some ' + req.getParameter(0) + '!');
}).get('/*', (res, req) => {
  /* Wildcards - make sure to catch them last */
  res.end('Nothing to see here!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
