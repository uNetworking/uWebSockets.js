/* This is an example of sync copying of large files.
 * NEVER DO THIS; ONLY FOR TESTING PURPOSES. WILL CAUSE
 * SEVERE BACKPRESSURE AND HORRIBLE PERFORMANCE.
 * Try navigating to the adderss with Chrome and see the video
 * in real time. */

const uWS = require('uWebSockets.js');
const fs = require('fs');

const port = 9001;
const fileName = 'spritefright.mp4';
const videoFile = toArrayBuffer(fs.readFileSync(fileName));
const totalSize = videoFile.byteLength;

console.log('WARNING: NEVER DO LIKE THIS; WILL CAUSE HORRIBLE BACKPRESSURE!');
console.log('Video size is: ' + totalSize + ' bytes');

const CHUNK_SIZE = 1024 * 64;

/* Helper function converting Node.js buffer to ArrayBuffer */
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

/* Yes, you can easily swap to SSL streaming by uncommenting here */
const app = uWS.App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'

}).get('/spritefright.mp4', (res, req) => {
    let aborted = false;
    let written = 0;

    res.onAborted(() => {
        aborted = true;
    });

    const stream = () => {
        let ok = true;

        // Use res.cork to pack multiple small writes into one syscall
        res.cork(() => {
            while (ok && written < videoFile.byteLength) {
                let chunk = videoFile.slice(written, Math.min(written + CHUNK_SIZE, videoFile.byteLength));
                
                // If this is the last chunk, use end() instead of write()
                if (written + chunk.byteLength === videoFile.byteLength) {
                    res.end(chunk);
                    written += chunk.byteLength;
                    ok = false; // Stop the loop
                } else {
                    ok = res.write(chunk);
                    written += chunk.byteLength;
                }
            }
        });

        return ok;
    };

    // Initial attempt to stream
    let ok = stream();

    // If we hit backpressure, set up the drain handler
    if (!ok && !aborted && written < videoFile.byteLength) {
        res.onWritable((offset) => {
            return stream();
        });
    }
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
