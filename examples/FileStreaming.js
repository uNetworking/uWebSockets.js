/* This is an example of streaming files */

const uWS = require('../dist/uws.js');
const fs = require('fs');
const port = 9001;

const smallFileType = 'application/json';
const smallFileName = 'absolutPathTo/smallFile.json';
const smallFileCachedBuffer = fs.readFileSync(smallFileName);
console.log('Small file size is: '+ smallFileCachedBuffer.length +' bytes');

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

    /* Send actual chunk to client */
    const sendChunkBuffer = () => {
        const [ok, done] = res.tryEnd(chunkBuffer, totalSize);
        if (done) {
            /* Streaming finished */
            readStream.destroy();
            onSucceed();
        } else if (ok) {
            /* Chunk send succeed */
            totalOffset += chunkBuffer.length;
            /* Resume stream if it was paused */
            readStream.resume();
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
        /* Always return if resend was successful or not */
        return sendChunkBuffer();
    });

    /* Register callback for stream events */
    readStream.on('error', (err) => {
        console.log('Error reading file: '+ err);
        /* res.close calls onAborted callback */
        res.cork(() => res.close());
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
    /* !! Use this only for small files !!
     * May cause server backpressure and bad performance
     * For bigger files you have to use streaming method */
    res.writeHeader('Content-Type', smallFileType).end(smallFileCachedBuffer);
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
