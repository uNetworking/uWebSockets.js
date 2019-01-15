/* This is an example of async streaming of large files.
 * Try navigating to the adderss with Chrome and see the video
 * in real time. */

const uWS = require('../dist/uws.js');
const fs = require('fs');
const crypto = require('crypto');

const port = 9001;
const fileName = '/home/alexhultman/Downloads/Sintel.2010.720p.mkv';
const totalSize = fs.statSync(fileName).size;

console.log('Video size is: ' + totalSize + ' bytes');

/* Helper function converting Node.js buffer to ArrayBuffer */
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

/* Returns true on success, false if it's having backpressure */
function tryStream(res, chunk, requestDataCb) {
  /* Stream as far as we can */
  let lastOffset = res.getWriteOffset();
  if (!res.tryEnd(chunk, totalSize)) {
    /* Save unsent chunk for when we can send it */
    res.chunk = chunk;
    res.chunkOffset = lastOffset;

    res.onWritable((offset) => {
      if (res.tryEnd(res.chunk.slice(offset - res.chunkOffset), totalSize)) {
        requestDataCb();
        return true;
      }
      return false;
    });
    /* Return failure */
    return false;
  }
  /* Return success */
  return true;
}

/* Yes, you can easily swap to SSL streaming by uncommenting here */
const app = uWS./*SSL*/App({
  key_file_name: '/home/alexhultman/key.pem',
  cert_file_name: '/home/alexhultman/cert.pem',
  passphrase: '1234'
}).get('/sintel.mkv', (res, req) => {
  /* Log */
  console.log("Streaming Sintel video...");

  /* Create read stream with Node.js and start streaming over Http */
  const readStream = fs.createReadStream(fileName);
  const hash = crypto.createHash('md5');
  readStream.on('data', (chunk) => {

    const ab = toArrayBuffer(chunk);
    hash.update(chunk);

    if (!tryStream(res, ab, () => {
      /* Called once we want more data */
      readStream.resume();
    })) {
      /* If we could not send this chunk, pause */
      readStream.pause();
    }
  }).on('end', () => {
    console.log("md5: " + hash.digest('hex'));
  });
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
