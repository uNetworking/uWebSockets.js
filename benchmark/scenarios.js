'use strict';

// The routes every arm serves and, per row of the table, the load sent to them: wrk for the http
// rows, load_test from the uWebSockets submodule for the ws rows (it always upgrades on "/").

const PRICE = JSON.stringify({
  status: 'success',
  asset: 'Gold Futures',
  symbol: 'GC=F',
  price: 4408.9,
  currency: 'USD',
});

const JSON_BODY = JSON.stringify({
  userId: 12345,
  action: 'purchase',
  items: Array.from({ length: 12 }, (_, i) => ({
    id: `A1B2C${i}`,
    name: 'Wireless Mouse',
    price: 25.99,
    quantity: i + 1,
  })),
  payment: { method: 'credit_card', transactionId: 'XYZ987654321', status: 'approved' },
});

const routes = (app, stats) => {
  app.get('/hello', (res) => {
    res.end('Hello World!');
  });

  app.get('/headers', (res, req) => {
    const agent = req.getHeader('user-agent');
    const accept = req.getHeader('accept');
    const id = req.getHeader('x-request-id');
    const url = req.getUrl();
    const q = req.getQuery('q');
    res.writeHeader('Content-Type', 'text/plain')
      .writeHeader('X-Request-Id', id)
      .writeHeader('Cache-Control', 'no-store')
      .end(`${url} ${q} ${agent} ${accept}`);
  });

  app.post('/json', (res) => {
    let body;
    res.onAborted(() => {
      res.aborted = true;
    });
    res.onData((chunk, isLast) => {
      body = body ? Buffer.concat([body, Buffer.from(chunk)]) : Buffer.from(chunk);
      if (isLast) {
        res.end(String(JSON.parse(body).items.length));
      }
    });
  });

  app.get('/cached', (res) => {
    res.end(PRICE);
  }, { lowerExpiry: 1, upperExpiry: 5 });

  app.ws('/*', {
    open: () => {
      stats.opened++;
    },
    message: (ws, message, isBinary) => {
      stats.messages++;
      ws.send(message, isBinary);
    },
  });
};

const scenarios = [
  { name: 'http/hello-world', tool: 'wrk', path: '/hello' },
  {
    name: 'http/headers',
    tool: 'wrk',
    path: '/headers?q=1',
    headers: { 'User-Agent': 'wrk', Accept: '*/*', 'X-Request-Id': 'abc123' },
  },
  {
    name: 'http/json-post-1kb',
    tool: 'wrk',
    path: '/json',
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON_BODY,
  },
  { name: 'http/cached', tool: 'wrk', path: '/cached' },
  { name: 'ws/echo-20b', tool: 'load_test', connections: 100, size: 20 },
  { name: 'ws/echo-4kb', tool: 'load_test', connections: 100, size: 4096 },
];

module.exports = { routes, scenarios };
