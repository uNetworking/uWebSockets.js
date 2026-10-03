/* Simple example of getting JSON from a POST */

import uWS from '../dist/index.mjs';
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).post('/*', (res, req) => {

  /* Note that you cannot read from req after returning from here */
  let url = req.getUrl();

  /* Read the body until done or error */
  readJson(res, (obj) => {
    console.log('Posted to ' + url + ': ');
    console.log(obj);

    res.end('Thanks for this json!');
  }, () => {
    /* Request was prematurely aborted or invalid or missing, stop reading */
    console.log('Invalid JSON or no data at all!');
  });

}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

/* Helper function for reading a posted JSON body */
function readJson(res, cb, err) {
  /* collectBody gives the whole body at once, or null if it is bigger than 1 MB */
  res.collectBody(1024 * 1024, (body) => {
    let json;
    try {
      json = JSON.parse(Buffer.from(body));
    } catch (e) {
      /* Too big, invalid or empty: res.close calls onAborted */
      res.close();
      return;
    }
    cb(json);
  });

  /* Register error cb */
  res.onAborted(err);
}
