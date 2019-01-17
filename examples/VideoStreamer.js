/* This is an example of async streaming of large files.
 * Try navigating to the adderss with Chrome and see the video
 * in real time. */

const uWS = require('../dist/uws.js');
const fs = require('fs');

const port = 9001;
const fileName = '/home/alexhultman/Downloads/Sintel.2010.720p.mkv';
const totalSize = fs.statSync(fileName).size;

console.log('Video size is: ' + totalSize + ' bytes');

/* Helper function converting Node.js buffer to ArrayBuffer */
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

/* Helper function to pipe the ReadaleStream over an Http responses */
function pipeStreamOverResponse(res, readStream, totalSize) {
  /* Careful! If Node.js would emit error before the first res.tryEnd, res will hang and never time out */
  /* For this demo, I skipped checking for Node.js errors, you are free to PR fixes to this example */
  readStream.on('data', (chunk) => {
    /* We only take standard V8 units of data */
    const ab = toArrayBuffer(chunk);

    /* Stream as far as we can */
    let lastOffset = res.getWriteOffset();
    if (!res.tryEnd(ab, totalSize)) {
      /* If we could not send this chunk, pause */
      readStream.pause();

      /* Save unsent chunk for when we can send it */
      res.ab = ab;
      res.abOffset = lastOffset;

      /* Register async handlers for drainage */
      res.onWritable((offset) => {
        if (res.tryEnd(res.ab.slice(offset - res.abOffset), totalSize)) {
          readStream.resume();
          return true;
        }
        return false;
      });
    }
  }).on('end', () => {
    /* Todo: handle errors of the stream, probably good to simply close the response */
  });

  /* If you plan to asyncronously respond later on, you MUST listen to onAborted BEFORE returning */
  res.onAborted(() => {
    console.log('Res is no longer valid!');
    readStream.destroy();
  });
}

/* Yes, you can easily swap to SSL streaming by uncommenting here */
const app = uWS.SSLApp({
  key_file_name: '/home/alexhultman/key.pem',
  cert_file_name: '/home/alexhultman/cert.pem',
  passphrase: '1234'
}).get('/sintel.mkv', (res, req) => {
  /* Log */
  console.log("Streaming Sintel video...");
  /* Create read stream with Node.js and start streaming over Http */
  const readStream = fs.createReadStream(fileName);
  pipeStreamOverResponse(res, readStream, totalSize);
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
