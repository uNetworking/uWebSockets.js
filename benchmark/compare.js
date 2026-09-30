'use strict';

// A/B of two builds: the dist of --head over the dist of --base, scenario by scenario, on the
// same machine in the same run. Two processes per arm: base2/base and head2/head are the same
// code, and how far they get from 1.0 is the noise of the run a head/base ratio has to clear.

const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const { execFileSync, fork, spawn } = require('child_process');
const { scenarios, REQUEST_HEADERS } = require('./scenarios');

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

const startArm = (dir, cpus, scenario) => {
  const child = fork(path.join(__dirname, 'server.js'), [], {
    env: { ...process.env, UWS_BENCH_MODULE: path.resolve(dir), UWS_BENCH_SCENARIO: scenario.name },
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
    const options = { host: '127.0.0.1', port, path: '/', method: scenario.method, headers: REQUEST_HEADERS, agent: false };
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

const pinned = (options, bin, args) => (options.cpus ? ['taskset', ['-c', options.cpus.load, bin, ...args]] : [bin, args]);

const generator = (arm, scenario, options) => {
  const target = [options.connections, '127.0.0.1', arm.port].map(String);
  if (scenario.tool === 'load_test') return [options.loadTest, [...target, '0', '0', String(scenario.size)]];
  return [options.httpLoadTest, scenario.method === 'POST' ? [...target, '1', '1'] : target];
};

// the generator is started, left alone for the round and killed: what it sent is counted on the
// server, by the http handler or by the ws message handler
const drive = async (arm, scenario, seconds, options) => {
  const before = await arm.send({ type: 'sample' });
  const child = spawn(...pinned(options, ...generator(arm, scenario, options)), { stdio: 'ignore' });
  let exited = false;
  const exit = new Promise((resolve) => child.on('exit', () => resolve((exited = true))));
  try {
    // load_test opens every ws connection before it sends; http_load_test sends on each one as it
    // opens, and opens them one after the other in a few ms
    const deadline = Date.now() + 10000;
    for (let up = false; !up; ) {
      if (exited || Date.now() > deadline) throw new Error(`${scenario.name}: the load did not start on port ${arm.port}`);
      await sleep(50);
      const now = await arm.send({ type: 'sample' });
      up = scenario.tool === 'load_test' ? now.opened - before.opened >= options.connections : now.requests > before.requests;
    }
    if (scenario.tool !== 'load_test') await sleep(200);
    const started = process.hrtime.bigint();
    const start = await arm.send({ type: 'sample' });
    await sleep(seconds * 1000);
    const end = await arm.send({ type: 'sample' });
    if (exited) throw new Error(`${scenario.name}: the load generator exited during the round`);
    const elapsed = Number(process.hrtime.bigint() - started) / 1e9;
    const requests = end.requests - start.requests;
    return { requests, rate: requests / elapsed, cpu: end.cpu - start.cpu, elapsed };
  } finally {
    child.kill('SIGKILL');
    await exit;
  }
};

// a cached answer runs no JS, so the server cannot count it: http_load_test prints its own count
// every 4 seconds, on the uSockets sweep timer, line buffered through stdbuf. The first print
// covers the start and only marks where the round begins
const driveCounted = async (arm, scenario, seconds, options) => {
  const [bin, args] = generator(arm, scenario, options);
  const child = spawn(...pinned(options, 'stdbuf', ['-oL', bin, ...args]), { stdio: ['ignore', 'pipe', 'ignore'] });
  let exited = false;
  const exit = new Promise((resolve) => child.on('exit', () => resolve((exited = true))));
  const want = 1 + Math.max(2, Math.ceil(seconds / 4));
  const prints = [];
  try {
    await new Promise((resolve, reject) => {
      const fail = (why) => reject(new Error(`${scenario.name}: ${why} after ${prints.length} of ${want} prints of http_load_test`));
      const timer = setTimeout(() => fail('timeout'), (want + 2) * 4000 + 10000);
      let rest = '';
      let chain = Promise.resolve();
      child.stdout.on('data', (chunk) => {
        const lines = (rest + chunk).split('\n');
        rest = lines.pop();
        for (const line of lines) {
          const match = /Req\/sec: ([\d.]+)/.exec(line);
          if (!match) continue;
          const at = process.hrtime.bigint();
          chain = chain
            .then(async () => {
              if (prints.length >= want) return;
              prints.push({ rate: Number(match[1]), at, cpu: (await arm.send({ type: 'sample' })).cpu });
              if (prints.length === want) {
                clearTimeout(timer);
                resolve();
              }
            })
            .catch(reject);
        }
      });
      child.on('exit', () => {
        if (prints.length < want) {
          clearTimeout(timer);
          fail('exit');
        }
      });
    });
  } finally {
    child.kill('SIGKILL');
    await exit;
  }
  const [first, ...counted] = prints;
  const last = counted[counted.length - 1];
  const elapsed = Number(last.at - first.at) / 1e9;
  const requests = counted.reduce((sum, print) => sum + print.rate * 4, 0);
  return { requests, rate: requests / elapsed, cpu: last.cpu - first.cpu, elapsed };
};

const measure = async (arm, scenario, seconds, options) => {
  const sample = scenario.cached ? await driveCounted(arm, scenario, seconds, options) : await drive(arm, scenario, seconds, options);
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
      `each one, from a copy of its build, after the other, in an order that changes every round. "Head / base" is the median of the per-round ` +
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
      `http_load_test and load_test with ${options.connections} connections.`
  );
  return lines.join('\n') + '\n';
};

const main = async () => {
  const args = parseArgs(process.argv.slice(2));
  if (!args.base || !args.head) {
    throw new Error(
      'usage: node benchmark/compare.js --base <dir> --head <dir> [--rounds 4] [--duration 3] [--connections 100] ' +
        '[--http-load-test <binary>] [--load-test <binary>] [--scenario <name>] [--output <file>]'
    );
  }
  const options = {
    rounds: Number(args.rounds || 4),
    duration: Number(args.duration || 3),
    warmup: Number(args.warmup || 1),
    connections: Number(args.connections || 100),
    httpLoadTest: args['http-load-test'] || path.join(args.head, 'uWebSockets/uSockets/http_load_test'),
    loadTest: args['load-test'] || path.join(args.head, 'uWebSockets/benchmarks/load_test'),
    cpus: layout(),
  };
  const labels = { base: label(args.base), head: label(args.head) };
  // the second process of each arm loads a copy of its build: the same bytes in other pages of
  // memory, so the noise band also sees what the placement of the code alone does
  const copy = (dir, name) => {
    const to = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'uws-benchmark-')), name);
    fs.cpSync(path.join(dir, 'dist'), path.join(to, 'dist'), { recursive: true });
    return to;
  };
  const dirs = { base: args.base, head: args.head, base2: copy(args.base, 'base2'), head2: copy(args.head, 'head2') };
  const names = Object.keys(dirs);
  const notes = [];
  const rows = [];
  for (const scenario of scenarios) {
    if (args.scenario && scenario.name !== args.scenario) continue;
    const bin = scenario.tool === 'load_test' ? options.loadTest : options.httpLoadTest;
    if (!fs.existsSync(bin)) {
      notes.push(`${scenario.name}: not run, no ${scenario.tool} binary at ${bin}`);
      continue;
    }
    process.stderr.write(`${scenario.name}\n`);
    // every scenario serves "/", so it gets its own four servers
    const arms = {};
    for (const name of names) arms[name] = startArm(dirs[name], options.cpus?.server, scenario);
    for (const name of names) arms[name].port = (await arms[name].ready).port;
    if (scenario.tool === 'http_load_test') await checkSame(scenario, arms);
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
    await Promise.all(names.map((name) => arms[name].send({ type: 'end' })));
  }

  for (const name of ['base2', 'head2']) fs.rmSync(path.dirname(dirs[name]), { recursive: true, force: true });

  const summary = markdown(labels, rows, notes, options);
  process.stdout.write(summary);
  if (args.output) fs.writeFileSync(args.output, summary);
};

main().catch((err) => {
  process.stderr.write(`${err.stack || err}\n`);
  process.exit(1);
});
