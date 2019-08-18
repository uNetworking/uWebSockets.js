/* Simple example of getting JSON from a POST */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).post('/*', (res, req) => {

  /* onAborted needs to be set if doing anything async like reading body */
  res.onAborted(() => res.aborted = true);

  /* Note that you cannot read from req after returning from here */
  let url = req.getUrl();

  /* Read the body until done */
  readBody(res, (body) => {

    /* body text as string */
    console.log('body text:', body.toString());

    /* parse json */
    try { var obj = JSON.parse(body); }
    catch(e) { console.log('json parse error'); }

    console.log('Posted to ' + url + ': ');
    console.log(obj);

    /* if response was aborted can't use it */
    if (res.aborted) return;

    if (obj) {
      res.end('Thanks for this json!');
    } else {
      res.writeStatus('400').end('invalid json');
    }
  });

}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

/* Helper function for reading a posted body */
function readBody(res, cb) {
  let body;
  res.onData((arrayBuffer, isLast) => {
    const chunk = Buffer.from(arrayBuffer);
    body = body ? Buffer.concat([body, chunk]) : isLast ? chunk : Buffer.from(chunk);
    if (isLast) cb(body);
  });
}
