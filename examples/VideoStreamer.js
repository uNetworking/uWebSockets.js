/* This is an example of async streaming of large files.
 * Try navigating to the adderss with Chrome and see the video
 * in real time. */

const uWS = require('../dist/uws.js');
const fs = require('fs');

const port = 9001;
const fileName = 'C:\\Users\\Alex\\Downloads\\Sintel.2010.720p.mkv';
const totalSize = fs.statSync(fileName).size;

let openStreams = 0;
let streamIndex = 0;

console.log('Video size is: ' + totalSize + ' bytes');

/* Helper function converting Node.js buffer to ArrayBuffer */
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

/* Either onAborted or simply finished request */
function onAbortedOrFinishedResponse(res, readStream) {

  if (res.id == -1) {
    console.log("ERROR! onAbortedOrFinishedResponse called twice for the same res!");
  } else {
    console.log('Stream was closed, openStreams: ' + --openStreams);
    console.timeEnd(res.id);
    readStream.destroy();
  }

  /* Mark this response already accounted for */
  res.id = -1;
}

/* Helper function to pipe the ReadaleStream over an Http responses */
function pipeStreamOverResponse(res, readStream, totalSize) {
  /* Careful! If Node.js would emit error before the first res.tryEnd, res will hang and never time out */
  /* For this demo, I skipped checking for Node.js errors, you are free to PR fixes to this example */
  readStream.on('data', (chunk) => {
    /* We only take standard V8 units of data */
    const ab = toArrayBuffer(chunk);

    /* Store where we are, globally, in our response */
    let lastOffset = res.getWriteOffset();

    /* Streaming a chunk returns whether that chunk was sent, and if that chunk was last */
    let [ok, done] = res.tryEnd(ab, totalSize);

    /* Did we successfully send last chunk? */
    if (done) {
      onAbortedOrFinishedResponse(res, readStream);
    } else if (!ok) {
      /* If we could not send this chunk, pause */
      readStream.pause();

      /* Save unsent chunk for when we can send it */
      res.ab = ab;
      res.abOffset = lastOffset;

      /* Register async handlers for drainage */
      res.onWritable((offset) => {
        /* Here the timeout is off, we can spend as much time before calling tryEnd we want to */

        /* On failure the timeout will start */
        let [ok, done] = res.tryEnd(res.ab.slice(offset - res.abOffset), totalSize);
        if (done) {
          onAbortedOrFinishedResponse(res, readStream);
        } else if (ok) {
          /* We sent a chunk and it was not the last one, so let's resume reading.
           * Timeout is still disabled, so we can spend any amount of time waiting
           * for more chunks to send. */
          readStream.resume();
        }

        /* We always have to return true/false in onWritable.
         * If you did not send anything, return true for success. */
        return ok;
      });
    }

  }).on('error', () => {
    /* Todo: handle errors of the stream, probably good to simply close the response */
    console.log('Unhandled read error from Node.js, you need to handle this!');
  });

  /* If you plan to asyncronously respond later on, you MUST listen to onAborted BEFORE returning */
  res.onAborted(() => {
    onAbortedOrFinishedResponse(res, readStream);
  });
}

/* Yes, you can easily swap to SSL streaming by uncommenting here */
const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/sintel.mkv', (res, req) => {
  /* Log */
  console.time(res.id = ++streamIndex);
  console.log('Stream was opened, openStreams: ' + ++openStreams);
  /* Create read stream with Node.js and start streaming over Http */
  res.writeStatus('200');
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
