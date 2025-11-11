/* This is an example of streaming files */

const uWS = require('../dist/uws.js');
const fs = require('fs');
const port = 9001;

/* Small file is cached in memory */
const smallFileType = 'application/json';
const smallFileName = 'absolutPathTo/smallFile.json';
const smallFileCachedBuffer = fs.readFileSync(smallFileName);
console.log('Small file size is: '+ smallFileCachedBuffer.length +' bytes');

/* Big file is streamed from storage */
const bigFileType = 'video/mpeg';
const bigFileName = 'absolutPathTo/bigFile.mp3';
const bigFileSize = fs.statSync(bigFileName).size;
console.log('Big file size is: '+ bigFileSize +' bytes');

let lastStreamIndex = 0;
let openStreams = 0;

/* Helper function to stream data */
/** @param {import('node:Stream').Readable} readStream */
const streamData = (res, readStream, totalSize, onSucceed) => {
    let chunkBuffer; /* Actual chunk being streamed */
    let totalOffset = 0; /* Actual chunk offset */

    /* Function to send actual chunk */
    const sendChunkBuffer = () => {
        const [ok, done] = res.tryEnd(chunkBuffer, totalSize);
        if (done) {
            /* Streaming finished */
            readStream.destroy();
            onSucceed();
        } else if (ok) {
            /* Chunk send succeed */
            totalOffset += chunkBuffer.length;
        } else {
            /* Chunk send failed (client backpressure)
             * onWritable will be called once client ready to receive new chunk
             * Pause stream to wait client */
            readStream.pause();
        }
        return ok;
    };

    /* Register onWritable callback
     * Will be called to drain client backpressure */
    res.onWritable((offset) => {
        if (offset !== totalOffset) {
            /* If start of the chunk was successfully sent
             * We only send the missing part */
            chunkBuffer = chunkBuffer.subarray(offset - totalOffset);
            totalOffset = offset;
        }
        if (sendChunkBuffer()) {
            /* Resume stream if resend succeed */
            readStream.resume();
            return true;
        }
        return false;
    });

    /* Register callback for stream events */
    readStream.on('error', (err) => {
        console.log('Error reading file: '+ err);
        /* res.close() calls onAborted callback */
        res.close();
    }).on('data', (newChunkBuffer) => {
        chunkBuffer = newChunkBuffer;
        /* Cork before sending new chunk */
        res.cork(sendChunkBuffer);
    });
};

const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/smallFile', (res, req) => {
    res.writeHeader('Content-Type', smallFileType);
    /* !! Use this only for small-sized bodies !!
     * May cause server backpressure and bad performance
     * For large bodies you must use the streaming method */
    res.end(smallFileCachedBuffer);
}).get('/bigFile', (res, req) => {
    const streamIndex = ++ lastStreamIndex;
    console.log('Stream ('+ streamIndex +') was opened, openStreams: '+ (++ openStreams));
    const readStream = fs.createReadStream(bigFileName);
    /* Attach onAborted handler because streaming is async */
    res.onAborted(() => {
        readStream.destroy();
        console.log('Stream ('+ streamIndex +') failed, openStreams: '+ (-- openStreams));
    });
    res.writeHeader('Content-Type', bigFileType);
    streamData(res, readStream, bigFileSize, () => {
        console.log('Stream ('+ streamIndex +') succeed, openStreams: '+ (-- openStreams));
    });
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});
