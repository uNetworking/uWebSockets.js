/* This example shows two different approaches to multi-core load balancing.
 * The first approach (the oldest) requires Linux and will only work on Linux.
 * This approach listens to port 4000 on all CPUs. That's it. That's all you do.
 * Listening to the same port from many worker threads will work on Linux.
 * The second approach will work on all platforms; you set up a main acceptorApp and register all child apps
 * (worker apps) with it. The acceptorApp will listen to port 9001 and move sockets in round-robin fashion to
 * the registered child apps.
 * Note that, in this example we only create 2 worker threads. Ideally you should create as many as there are CPUs
 * in your system. But by only creating 2 here, it is simple to see the perf. gain on a system of 4 cores, as you can then
 * run the client side on the remaining 2 cores without interfering with the server side. */

const uWS = require('../dist/uws.js');
const port = 9001;
const { Worker, isMainThread, threadId, parentPort } = require('worker_threads');
const os = require('os');

if (isMainThread) {

	/* The acceptorApp only listens, but must be SSL if worker apps are SSL and likewise opposite */
	const acceptorApp = uWS./*SSL*/App({
		key_file_name: 'misc/key.pem',
		cert_file_name: 'misc/cert.pem',
		passphrase: '1234'
	  }).listen(port, (token) => {
		if (token) {
		  console.log('Listening to port ' + port + ' from thread ' + threadId + ' as main acceptor');
		} else {
		  console.log('Failed to listen to port ' + port + ' from thread ' + threadId);
		}
	  });

  /* Main thread loops over all CPUs */
  /* In this case we only spawn two (hardcoded) */
  /*os.cpus()*/[0, 1].forEach(() => {
	
    /* Spawn a new thread running this source file */
    new Worker(__filename).on("message", (workerAppDescriptor) => {
		acceptorApp.addChildAppDescriptor(workerAppDescriptor);
	});
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
  }).listen(4000, (token) => {
	if (token) {
		console.log('Listening to port ' + 4000 + ' from thread ' + threadId);
	  } else {
		console.log('Failed to listen to port ' + 4000 + ' from thread ' + threadId);
	  }
  });

  /* The worker sends back its descriptor to the main acceptor */
  parentPort.postMessage(app.getDescriptor());
}
