'use strict';

// A/B of two builds: the dist of --head over the dist of --base, scenario by scenario, on the
// same machine in the same run. Two processes per arm: base2/base and head2/head are the same
// code, and how far they get from 1.0 is the noise of the run a head/base ratio has to clear.

const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const { execFile, execFileSync, fork, spawn } = require('child_process');
const { promisify } = require('util');
const { scenarios } = require('./scenarios');

// nothing is marked below this, whatever the noise said
const FLOOR = 0.02;

const parseArgs = (argv) => {
  const args = {};
  for (let i = 0; i < argv.length; i++) {
    if (argv[i].startsWith('--')) args[argv[i].slice(2)] = argv[++i];
  }
  return args;
};

const median = (values) => {
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const label = (dir) => {
  try {
    return execFileSync('git', ['-C', dir, 'rev-parse', '--short', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    return path.basename(path.resolve(dir));
  }
};

const cpuList = (text) =>
  text.trim().split(',').flatMap((part) => {
    const [from, to = from] = part.split('-').map(Number);
    return Array.from({ length: to - from + 1 }, (_, i) => from + i);
  });

// linux: the server gets a cpu whose hyperthread sibling stays idle, the load generator the rest
const layout = () => {
  try {
    const allowed = cpuList(/Cpus_allowed_list:\s*(\S+)/.exec(fs.readFileSync('/proc/self/status', 'utf8'))[1]);
    const server = allowed[0];
    const siblings = cpuList(fs.readFileSync(`/sys/devices/system/cpu/cpu${server}/topology/thread_siblings_list`, 'utf8'));
    const load = allowed.filter((cpu) => !siblings.includes(cpu));
    execFileSync('taskset', ['-V'], { stdio: 'ignore' });
    return load.length ? { server: String(server), load: load.join(',') } : null;
  } catch {
    return null;
  }
};

const startArm = (dir, cpus) => {
  const child = fork(path.join(__dirname, 'server.js'), [], {
    env: { ...process.env, UWS_BENCH_MODULE: path.resolve(dir) },
  });
  if (cpus) execFileSync('taskset', ['-acp', cpus, String(child.pid)], { stdio: 'ignore' });
  const waiting = [];
  child.on('message', (msg) => {
    const { resolve, reject } = waiting.shift();
    msg.ok ? resolve(msg) : reject(new Error(msg.error));
  });
  child.on('exit', (code) => {
    for (const { reject } of waiting.splice(0)) reject(new Error(`server for ${dir} exited with ${code}`));
  });
  const ready = new Promise((resolve, reject) => waiting.push({ resolve, reject }));
  return {
    ready,
    send: (msg) =>
      new Promise((resolve, reject) => {
        waiting.push({ resolve, reject });
        child.send(msg);
      }),
  };
};

const request = (port, scenario) =>
  new Promise((resolve, reject) => {
    const options = { host: '127.0.0.1', port, path: scenario.path, method: scenario.method, headers: scenario.headers, agent: false };
    const req = http.request(options, (res) => {
      const chunks = [];
      res.on('data', (chunk) => chunks.push(chunk));
      res.on('end', () => resolve({ status: res.statusCode, body: Buffer.concat(chunks).toString() }));
    });
    req.on('error', reject);
    req.end(scenario.body);
  });

const checkSame = async (scenario, arms) => {
  const answers = [];
  for (const [name, arm] of Object.entries(arms)) {
    answers.push({ name, ...(await request(arm.port, scenario)) });
  }
  const [first, ...others] = answers;
  if (first.status !== 200) throw new Error(`${scenario.name}: ${first.name} answered ${first.status} ${first.body}`);
  for (const other of others) {
    if (other.status !== first.status || other.body !== first.body) {
      throw new Error(`${scenario.name}: ${other.name} answered ${other.status} ${other.body}, ${first.name} answered ${first.status} ${first.body}`);
    }
  }
};

const luaScript = (scenario, dir) => {
  if (!scenario.method && !scenario.body && !scenario.headers) return null;
  const lines = [];
  if (scenario.method) lines.push(`wrk.method = "${scenario.method}"`);
  if (scenario.body) lines.push(`wrk.body = [==[${scenario.body}]==]`);
  for (const [key, value] of Object.entries(scenario.headers || {})) lines.push(`wrk.headers["${key}"] = "${value}"`);
  const file = path.join(dir, `${scenario.name.replace(/\W/g, '_')}.lua`);
  fs.writeFileSync(file, `${lines.join('\n')}\n`);
  return file;
};

const pinned = (options, bin, args) => (options.cpus ? ['taskset', ['-c', options.cpus.load, bin, ...args]] : [bin, args]);

const wrk = async (arm, scenario, seconds, options) => {
  const args = ['-t', String(options.threads), '-c', String(options.connections), '-d', `${seconds}s`];
  if (scenario.script) args.push('-s', scenario.script);
  args.push(`http://127.0.0.1:${arm.port}${scenario.path}`);
  const { stdout } = await promisify(execFile)(...pinned(options, options.wrk, args));
  const requests = /(\d+) requests in/.exec(stdout);
  const rate = /Requests\/sec:\s+([\d.]+)/.exec(stdout);
  if (!requests || !rate || /Non-2xx or 3xx/.test(stdout)) throw new Error(`${scenario.name} on port ${arm.port}:\n${stdout}`);
  const errors = /Socket errors: (.*)/.exec(stdout);
  return { requests: Number(requests[1]), rate: Number(rate[1]), errors: errors && errors[1] };
};

// load_test is started, left alone for the round and killed: what it sent is counted on the server
const loadTest = async (arm, scenario, seconds, options) => {
  const before = await arm.send({ type: 'sample' });
  const args = [scenario.connections, '127.0.0.1', arm.port, 0, 0, scenario.size].map(String);
  const child = spawn(...pinned(options, options.loadTest, args), { stdio: 'ignore' });
  let exited = false;
  const exit = new Promise((resolve) => child.on('exit', () => resolve((exited = true))));
  try {
    const deadline = Date.now() + 10000;
    for (let opened = 0; opened < scenario.connections; ) {
      if (exited || Date.now() > deadline) throw new Error(`${scenario.name}: load_test brought up ${opened} of ${scenario.connections} connections`);
      await sleep(50);
      opened = (await arm.send({ type: 'sample' })).opened - before.opened;
    }
    const started = process.hrtime.bigint();
    const start = await arm.send({ type: 'sample' });
    await sleep(seconds * 1000);
    const end = await arm.send({ type: 'sample' });
    if (exited) throw new Error(`${scenario.name}: load_test exited during the round`);
    const elapsed = Number(process.hrtime.bigint() - started) / 1e9;
    const requests = end.messages - start.messages;
    return { requests, rate: requests / elapsed, cpu: end.cpu - start.cpu, elapsed };
  } finally {
    child.kill('SIGKILL');
    await exit;
  }
};

const measure = async (arm, scenario, seconds, options) => {
  let sample;
  if (scenario.tool === 'wrk') {
    const started = process.hrtime.bigint();
    const start = await arm.send({ type: 'sample' });
    sample = await wrk(arm, scenario, seconds, options);
    const end = await arm.send({ type: 'sample' });
    sample.cpu = end.cpu - start.cpu;
    sample.elapsed = Number(process.hrtime.bigint() - started) / 1e9;
  } else {
    sample = await loadTest(arm, scenario, seconds, options);
  }
  sample.cost = sample.cpu / sample.requests;
  sample.busy = sample.cpu / 1e6 / sample.elapsed;
  return sample;
};

const judge = (ratios, same) => {
  const value = median(ratios);
  const min = Math.min(...ratios);
  const max = Math.max(...ratios);
  const noise = Math.max(...same.map((ratio) => Math.abs(ratio - 1)));
  return { value, min, max, noise, notable: Math.abs(value - 1) >= Math.max(noise, FLOOR) && (min > 1 || max < 1) };
};

const percent = (ratio) => `${ratio >= 1 ? '+' : ''}${((ratio - 1) * 100).toFixed(1)}%`;

const markdown = (labels, rows, notes, options) => {
  const lines = ['<!-- benchmark-comment -->', '', `## Benchmark: \`${labels.head}\` against \`${labels.base}\``, ''];
  lines.push('| Scenario | Base req/s | Head req/s | Head / base | Rounds | Noise | Busy | CPU per request |');
  lines.push('| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |');
  for (const row of rows) {
    const mark = row.rate.notable ? (row.rate.value < 1 ? ' :eyes:' : ' :trophy:') : '';
    const change = row.rate.notable ? `**${percent(row.rate.value)}**` : percent(row.rate.value);
    const cost = row.cost.notable ? `**${row.cost.value.toFixed(3)}x**` : `${row.cost.value.toFixed(3)}x`;
    lines.push(
      `| ${row.name}${mark} | ${row.base.toFixed(0)} | ${row.head.toFixed(0)} | ` +
        `${row.rate.value.toFixed(3)}x (${change}) | ${row.rate.min.toFixed(2)} to ${row.rate.max.toFixed(2)} | ` +
        `±${(row.rate.noise * 100).toFixed(1)}% | ${(row.busyBase * 100).toFixed(0)}% / ${(row.busyHead * 100).toFixed(0)}% | ` +
        `${row.costBase.toFixed(2)} / ${row.costHead.toFixed(2)} us (${cost}) |`
    );
  }
  lines.push('');
  lines.push(
    `${options.rounds} rounds of ${options.duration}s per scenario, each round loading base, head and a second process of ` +
      `each one after the other, in an order that changes every round. "Head / base" is the median of the per-round ` +
      `ratios of req/s and "Rounds" their range. "Noise" is how far base/base and head/head, the same code on both ` +
      `sides, got from 1 in this same run: that is what the machine did, so a row is marked only when the median ` +
      `moved further than that, at least ${Math.round(FLOOR * 100)}%, and every round moved the same way: :eyes: ` +
      `slower, :trophy: faster. "Busy" is the cpu time of the server process over the round and "CPU per request" ` +
      `that time divided by the requests it answered, with the head/base ratio judged against its own same-code ` +
      `band and bold when it cleared it. When busy is well under 100% the load generator set the pace, the req/s ` +
      `are its and the cpu per request is the column to read. Only the ratios are comparable across runs, the ` +
      `absolute figures depend on the runner.`
  );
  for (const note of notes) lines.push('', `- ${note}`);
  lines.push('');
  lines.push(
    `Node ${process.version}, ${os.cpus()[0]?.model || 'unknown cpu'}, ${os.cpus().length} cores` +
      `${options.cpus ? ` (server on cpu ${options.cpus.server}, load on ${options.cpus.load})` : ''}, ` +
      `wrk -t${options.threads} -c${options.connections}.`
  );
  return lines.join('\n') + '\n';
};

const main = async () => {
  const args = parseArgs(process.argv.slice(2));
  if (!args.base || !args.head) {
    throw new Error(
      'usage: node benchmark/compare.js --base <dir> --head <dir> [--rounds 4] [--duration 3] [--threads 2] ' +
        '[--connections 100] [--wrk wrk] [--load-test <binary>] [--scenario <name>] [--output <file>]'
    );
  }
  const options = {
    rounds: Number(args.rounds || 4),
    duration: Number(args.duration || 3),
    warmup: Number(args.warmup || 1),
    threads: Number(args.threads || 2),
    connections: Number(args.connections || 100),
    wrk: args.wrk || 'wrk',
    loadTest: args['load-test'] || path.join(args.head, 'uWebSockets/benchmarks/load_test'),
    cpus: layout(),
  };
  const labels = { base: label(args.base), head: label(args.head) };
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'uws-benchmark-'));

  const arms = {
    base: startArm(args.base, options.cpus?.server),
    head: startArm(args.head, options.cpus?.server),
    base2: startArm(args.base, options.cpus?.server),
    head2: startArm(args.head, options.cpus?.server),
  };
  const names = Object.keys(arms);
  for (const name of names) arms[name].port = (await arms[name].ready).port;

  const notes = [];
  const rows = [];
  for (const scenario of scenarios) {
    if (args.scenario && scenario.name !== args.scenario) continue;
    if (scenario.tool === 'load_test' && !fs.existsSync(options.loadTest)) {
      notes.push(`${scenario.name}: not run, no load_test binary at ${options.loadTest}`);
      continue;
    }
    process.stderr.write(`${scenario.name}\n`);
    if (scenario.tool === 'wrk') {
      await checkSame(scenario, arms);
      scenario.script = luaScript(scenario, dir);
    }
    const samples = Object.fromEntries(names.map((name) => [name, []]));
    for (let i = -1; i < options.rounds; i++) {
      // round -1 warms up cold code and is thrown away; the order rotates and flips every round
      const seconds = i < 0 ? options.warmup : options.duration;
      const rotated = names.map((_, k) => names[(k + i + 1) % names.length]);
      const order = i % 2 ? rotated.reverse() : rotated;
      for (const name of order) {
        const sample = await measure(arms[name], scenario, seconds, options);
        if (i >= 0) samples[name].push(sample);
      }
    }
    const ratio = (over, under, key) => samples[over].map((sample, i) => sample[key] / samples[under][i][key]);
    const same = (key) => [...ratio('base2', 'base', key), ...ratio('head2', 'head', key)];
    const rate = judge(ratio('head', 'base', 'rate'), same('rate'));
    const cost = judge(ratio('head', 'base', 'cost'), same('cost'));
    const of = (name, key) => median(samples[name].map((sample) => sample[key]));
    const errors = new Set(names.flatMap((name) => samples[name].map((sample) => sample.errors).filter(Boolean)));
    for (const error of errors) notes.push(`${scenario.name}: wrk reported socket errors, ${error}`);
    process.stderr.write(
      `  ${rate.value.toFixed(3)}x (${rate.min.toFixed(2)} to ${rate.max.toFixed(2)}), noise ±${(rate.noise * 100).toFixed(1)}%, ` +
        `busy ${(of('base', 'busy') * 100).toFixed(0)}% / ${(of('head', 'busy') * 100).toFixed(0)}%, ` +
        `${of('base', 'cost').toFixed(2)} / ${of('head', 'cost').toFixed(2)} us per request (${cost.value.toFixed(3)}x)\n`
    );
    rows.push({
      name: scenario.name,
      base: of('base', 'rate'),
      head: of('head', 'rate'),
      rate,
      cost,
      costBase: of('base', 'cost'),
      costHead: of('head', 'cost'),
      busyBase: of('base', 'busy'),
      busyHead: of('head', 'busy'),
    });
  }
  await Promise.all(names.map((name) => arms[name].send({ type: 'end' })));

  const summary = markdown(labels, rows, notes, options);
  process.stdout.write(summary);
  if (args.output) fs.writeFileSync(args.output, summary);
};

main().catch((err) => {
  process.stderr.write(`${err.stack || err}\n`);
  process.exit(1);
});
