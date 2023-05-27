/* Example of looping over all headers (not recommended as production solution,
 * do NOT use this to "solve" your problems with headers, use ONLY for debugging) */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/*', (res, req) => {

  res.write('<h2>Hello, your headers are:</h2><ul>');

  req.forEach((k, v) => {
    res.write('<li>');
    res.write(k);
    res.write(' = ');
    res.write(v);
    res.write('</li>');
  });

  res.end('</ul>');

}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
