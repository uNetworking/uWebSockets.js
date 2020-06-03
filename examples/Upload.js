/* Minimal POST example a SSL/non-SSL */
/* todo: proper throttling, better example */

/* a good example would be using the router to get
 * a file name and stream that file to disk */

const uWS = require('../dist/uws.js');
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).post('/*', (res, req) => {
  console.log('Posted to ' + req.getUrl());
  const contentLength = +req.getHeader("content-length");
  const bytes = new Uint8Array(contentLength);
  let offset = 0;
  res.onData((chunk, isLast) => {
    /* Buffer this anywhere you want to */
    console.log('Got chunk of data with length ' + chunk.byteLength + ', isLast: ' + isLast);
    bytes.set(new Uint8Array(chunk), offset);
    offset += chunk.byteLength;

    /* We respond when we are done */
    if (isLast) {
      res.end('Thanks for the data!');
      /* bytes contains the whole file as Uint8Array, you can get the buffer with Buffer.from(bytes)  */
    }
  });

  res.onAborted(() => {
    /* Request was prematurely aborted, stop reading */
    console.log('Eh, okay. Thanks for nothing!');
  });
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
