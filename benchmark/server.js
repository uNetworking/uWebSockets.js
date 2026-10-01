'use strict';

// One arm: serves the route of the scenario in UWS_BENCH_SCENARIO from the build under
// UWS_BENCH_MODULE, and reports its cpu time and counters to compare.js over IPC.

const fs = require('fs');
const path = require('path');
const { scenarios } = require('./scenarios');

const root = path.resolve(process.env.UWS_BENCH_MODULE);
// a base from before the rename to index.js still has dist/uws.js
const entry = ['dist/index.js', 'dist/uws.js'].find((file) => fs.existsSync(path.join(root, file)));
const uWS = require(path.join(root, entry));
const scenario = scenarios.find((s) => s.name === process.env.UWS_BENCH_SCENARIO);

const stats = { opened: 0, requests: 0 };
const app = uWS.App();
scenario.route(app, stats);

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
