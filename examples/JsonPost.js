/* Simple example of getting JSON from a POST */

const uWS = require('../dist/uws.js');
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
    let buffer;
    /* Register data cb */
    res.onData((ab, isLast) => {
        let chunk = Buffer.from(ab);
        if (isLast) {
            try {
                cb(JSON.parse((buffer ? Buffer.concat([buffer, chunk]) : chunk)));
            } catch (e) {
                /* res.close calls onAborted */
                res.close();
            }
        } else {
            buffer = Buffer.concat(buffer ? [buffer, chunk] : [chunk]);
        }
    });

    /* Register error cb */
    res.onAborted(err);
}
