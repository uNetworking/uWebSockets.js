/* This example spawns two worker threads, each with their own
 * server listening to the same port (Linux feature). */

const uWS = require('../dist/uws.js');
const port = 9001;
const { Worker, isMainThread, threadId } = require('worker_threads');
const os = require('os');

if (isMainThread) {
  /* Main thread loops over all CPUs */
  /* In this case we only spawn two (hardcoded) */
  /*os.cpus()*/[0, 1].forEach(() => {
    /* Spawn a new thread running this source file */
    new Worker(__filename);
  });

  /* I guess main thread joins by default? */
} else {
  /* Here we are inside a worker thread */
  const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
  }).get('/*', (res, req) => {
    res.end('Hello Worker!');
  }).listen(port, (token) => {
    if (token) {
      console.log('Listening to port ' + port + ' from thread ' + threadId);
    } else {
      console.log('Failed to listen to port ' + port + ' from thread ' + threadId);
    }
  });
}
