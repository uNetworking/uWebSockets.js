/* This is an example of sync copying of large files.
 * NEVER DO THIS; ONLY FOR TESTING PURPOSES. WILL CAUSE
 * SEVERE BACKPRESSURE AND HORRIBLE PERFORMANCE.
 * Try navigating to the adderss with Chrome and see the video
 * in real time. */

const uWS = require('../dist/uws.js');
const fs = require('fs');

const port = 9001;
const fileName = '/home/alexhultman/Downloads/Sintel.2010.720p.mkv';
const totalSize = fs.statSync(fileName).size;

console.log('WARNING: NEVER DO LIKE THIS; WILL CAUSE HORRIBLE BACKPRESSURE!');
console.log('Video size is: ' + totalSize + ' bytes');

/* Helper function converting Node.js buffer to ArrayBuffer */
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

/* Yes, you can easily swap to SSL streaming by uncommenting here */
const app = uWS.SSLApp({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/sintel.mkv', (res, req) => {
  /* Log */
  console.log("Copying Sintel video...");
  /* Copy the entire video to backpressure */
  res.end(toArrayBuffer(fs.readFileSync(fileName)));
}).get('/*', (res, req) => {
  /* Make sure to always handle every route */
  res.end('Nothing to see here!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
