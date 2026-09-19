'use strict';

// One arm: serves every scenario from the build under UWS_BENCH_MODULE and reports its cpu time
// and ws counters to compare.js over IPC.

const path = require('path');
const { routes } = require('./scenarios');

const root = path.resolve(process.env.UWS_BENCH_MODULE);
const uWS = require(path.join(root, 'dist/uws.js'));

const stats = { opened: 0, messages: 0 };
const app = uWS.App();
routes(app, stats);

app.listen('127.0.0.1', 0, (token) => {
  if (!token) {
    process.send({ ok: false, error: `${root}: listen failed` });
    process.exit(1);
  }
  process.send({ ok: true, port: uWS.us_socket_local_port(token) });
});

process.on('message', (msg) => {
  if (msg.type === 'sample') {
    const { user, system } = process.cpuUsage();
    process.send({ ok: true, cpu: user + system, ...stats });
  } else if (msg.type === 'end') {
    process.send({ ok: true }, () => process.exit(0));
  }
});
