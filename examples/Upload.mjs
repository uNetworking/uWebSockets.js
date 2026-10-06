/* Minimal POST example a SSL/non-SSL */
/* todo: proper throttling, better example */

/* a good example would be using the router to get
 * a file name and stream that file to disk */

import uWS from '../dist/index.mjs';
const port = 9001;

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).post('/*', (res, req) => {
  console.log('Posted to ' + req.getUrl());
  /* It also gets the abort, as res null, so no onAborted is needed */
  res.onDataOrAborted((res, chunk, maxRemainingBodyLength) => {
    if (!res) {
      /* Request was prematurely aborted, stop reading */
      console.log('Eh, okay. Thanks for nothing!');
      return;
    }

    /* Buffer this anywhere you want to */
    console.log('Got chunk of data with length ' + chunk.byteLength + ', maxRemainingBodyLength: ' + maxRemainingBodyLength);

    /* We respond when we are done */
    if (maxRemainingBodyLength === 0n) {
      res.end('Thanks for the data!');
    }
  });
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
