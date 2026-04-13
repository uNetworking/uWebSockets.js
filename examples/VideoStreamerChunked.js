/* This is an example stressing the Chunked transfer mode */
/* It is not the most optimal way to send lots of data, tryEnd is */
/* If you do not know the size of the file, this Chunked transfer mode
 * is necessary. We send 256kb chunks and wait for them to drain. The 
 * max backpressure is thus 256 kb. Again, tryEnd is more efficient if
 * you know the file size. The call "end" with a huge file is the worst
 * possible way, as it copies the entire file to backpressure. */

const uWS = require('uWebSockets.js');
const fs = require('fs');

const port = 9001;
const fileName = 'Sintel.2010.1080p.mkv';

/**
 * Optimized to return a Uint8Array view of the Node.js Buffer 
 * without creating a new ArrayBuffer copy.
 */
function getFileView(name) {
    const buffer = fs.readFileSync(name);
    // This creates a Uint8Array pointing to the EXACT same memory Node.js allocated
    return new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
}

const videoView = getFileView(fileName);
const totalSize = videoView.byteLength;

console.log('Video size is: ' + totalSize + ' bytes');

const CHUNK_SIZE = 1024 * 256;

const app = uWS.App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/spritefright.mkv', (res, req) => {
    let aborted = false;
    let written = 0;

    res.onAborted(() => {
        aborted = true;
    });

    const stream = () => {
        let ok = true;

        res.cork(() => {
            while (ok && written < totalSize) {
                const end = Math.min(written + CHUNK_SIZE, totalSize);
                
                /**
                 * .subarray() is a PURE VIEW. 
                 * No new memory is allocated for the file data here.
                 */
                const chunk = videoView.subarray(written, end);
                
                if (end === totalSize) {
                    res.write(chunk);
                    res.end();
                    written = totalSize;
                    ok = false;
                } else {
                    ok = res.write(chunk);
                    written = end;
                }
            }
        });

        return ok;
    };

    let ok = stream();

    if (!ok && !aborted && written < totalSize) {
        res.onWritable((offset) => {
            return stream();
        });
    }
}).get('/*', (res, req) => {
    res.end('Nothing to see here!');
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});