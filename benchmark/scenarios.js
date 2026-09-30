'use strict';

// The rows of the table: the route each server serves and the load sent to it. http_load_test from
// uSockets for the http rows, load_test from uWebSockets for the ws rows. Both always ask for "/",
// so every scenario gets its own servers with only its route on it. What was answered is counted
// on the server, except for the cached route, which runs no JS: there http_load_test counts.

// what http_load_test sends: its request is fixed, the POST body too
const REQUEST_HEADERS = { Host: 'localhost:3000', 'User-Agent': 'curl/7.68.0', Accept: '*/*' };
const POST_BODY = '{"key":13}';

const PRICE = JSON.stringify({
  status: 'success',
  asset: 'Gold Futures',
  symbol: 'GC=F',
  price: 4408.9,
  currency: 'USD',
});

const scenarios = [
  {
    name: 'http/hello-world',
    tool: 'http_load_test',
    route: (app, stats) => {
      app.get('/', (res) => {
        stats.requests++;
        res.end('Hello World!');
      });
    },
  },
  {
    name: 'http/headers',
    tool: 'http_load_test',
    route: (app, stats) => {
      app.get('/', (res, req) => {
        stats.requests++;
        const host = req.getHeader('host');
        const agent = req.getHeader('user-agent');
        const accept = req.getHeader('accept');
        const url = req.getUrl();
        res.writeHeader('Content-Type', 'text/plain')
          .writeHeader('X-Host', host)
          .writeHeader('Cache-Control', 'no-store')
          .end(`${url} ${agent} ${accept}`);
      });
    },
  },
  {
    name: 'http/json-post',
    tool: 'http_load_test',
    method: 'POST',
    body: POST_BODY,
    route: (app, stats) => {
      app.post('/', (res) => {
        stats.requests++;
        let body;
        res.onAborted(() => {
          res.aborted = true;
        });
        res.onData((chunk, isLast) => {
          body = body ? Buffer.concat([body, Buffer.from(chunk)]) : Buffer.from(chunk);
          if (isLast) {
            res.end(String(JSON.parse(body).key));
          }
        });
      });
    },
  },
  {
    // the handler runs about once a second, the rest is answered natively from the cache
    name: 'http/cached',
    tool: 'http_load_test',
    cached: true,
    route: (app) => {
      app.get('/', (res) => {
        res.end(PRICE);
      }, { lowerExpiry: 1, upperExpiry: 5 });
    },
  },
  { name: 'ws/echo-20b', tool: 'load_test', size: 20, route: wsEcho },
  { name: 'ws/echo-4kb', tool: 'load_test', size: 4096, route: wsEcho },
];

function wsEcho(app, stats) {
  app.ws('/*', {
    open: () => {
      stats.opened++;
    },
    message: (ws, message, isBinary) => {
      stats.requests++;
      ws.send(message, isBinary);
    },
  });
}

module.exports = { scenarios, REQUEST_HEADERS };
