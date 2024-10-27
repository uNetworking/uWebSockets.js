// This is an example of streaming files

const uWS = require('../dist/uws.js');
const fs = require('fs');

const port = 9001;
const bigFileName = 'absolutPathTo/bigVideo.mp3';
const bigFileSize = fs.statSync(bigFileName).size;
console.log('Video size is: '+ bigFileSize +' bytes');

// Stream data to res
/** @param {import('node:Stream').Readable} readStream */
const streamData = (res, readStream, totalSize, onFinished) => {
    let chunkBuffer; // Actual chunk being streamed
    let totalOffset = 0; // Actual chunk offset
    const sendChunkBuffer = () => {
        const [ok, done] = res.tryEnd(chunkBuffer, totalSize);
        if (done) {
            // Streaming finished
            readStream.destroy();
            onFinished();
        } else if (ok) {
            // Chunk send success
            totalOffset += chunkBuffer.length;
            // Resume stream if it was paused
            readStream.resume();
        } else {
            // Chunk send failed (client backpressure)
            // onWritable will be called once client ready to receive new chunk
            // Pause stream
            readStream.pause();
        }
        return ok;
    };

    // Attach onAborted handler because streaming is async
    res.onAborted(() => {
        readStream.destroy();
        onFinished();
    });

    // Register onWritable callback
    // Will be called to drain client backpressure
    res.onWritable((offset) => {
        const offsetDiff = offset - totalOffset;
        if (offsetDiff) {
            // If start of the chunk was successfully sent
            // We only send the missing part
            chunkBuffer = chunkBuffer.subarray(offsetDiff);
            totalOffset = offset;
        }
        // Always return if resend was successful or not
        return sendChunkBuffer();
    });

    // Register callback for stream events
    readStream.on('error', (err) => {
        console.log('Error reading file: '+ err);
        // res.close calls onAborted callback
        res.close();
    }).on('data', (newChunkBuffer) => {
        chunkBuffer = newChunkBuffer;
        // Cork before sending new chunk
        res.cork(sendChunkBuffer);
    });
};

let lastStreamIndex = 0;
let openStreams = 0;

const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/bigFile', (res, req) => {
    const streamIndex = ++ lastStreamIndex;
    console.log('Stream ('+ streamIndex +') was opened, openStreams: '+ (++ openStreams));
    res.writeHeader('Content-Type', 'video/mpeg');
    // Create read stream with fs.createReadStream
    streamData(res, fs.createReadStream(bigFileName), bigFileSize, () => {
        // On streaming finished (success/error/onAborted)
        console.log('Stream ('+ streamIndex +') was closed, openStreams: '+ (-- openStreams));
    });

}).get('/smallFile', (res, req) => {
    // !! Use this only for small files !!
    // May cause server backpressure and bad performance
    // For bigger files you have to use streaming
    try {
        const fileBuffer = fs.readFileSync('absolutPathTo/smallData.json');
        res.writeHeader('Content-Type', 'application/json').end(fileBuffer);
    } catch (err) {
        console.log('Error reading file: '+ err);
        res.writeStatus('500 Internal Server Error').end();
    }
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});
