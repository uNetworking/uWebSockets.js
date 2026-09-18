/* The routes a plaintext/JSON benchmark hits, written the way uWS wants them:
 * DeclarativeResponse where no JavaScript has to run, microcaching where the
 * answer can be reused, and collectBody where a body has to be read. */

import uWS from '../dist/uws.js';
const port = 3000;

/* Bodies larger than this are refused rather than buffered. */
const maxBodySize = 1024 * 1024;

uWS.App().get('/', new uWS.DeclarativeResponse()
  .writeHeader('content-type', 'text/plain')
  .end('Hi')
).get('/id/:id', new uWS.DeclarativeResponse()
  .writeHeader('content-type', 'text/plain')
  .writeHeader('x-powered-by', 'benchmark')
  .writeParameterValue('id')
  .write(' ')
  .writeQueryValue('name')
  .end()
).get('/db', (res, req) => {
  /* Stands in for the database round trip of the benchmark. Microcaching runs
   * this handler at most once per second; every other request inside that
   * window is answered from the cache, without entering JavaScript at all.
   * A cached response carries end, cork and onAborted, and nothing else. */
  res.end(JSON.stringify({
    id: 1 + Math.floor(Math.random() * 10000),
    randomNumber: 1 + Math.floor(Math.random() * 10000)
  }));
}, {
  /* This object specifies the caching options, in seconds */
  lowerExpiry: 1,
  upperExpiry: 5
}).post('/json', (res, req) => {
  /* The response outlives this function, so an abort handler is mandatory.
   * It must not touch res: once it fires, res is already gone. */
  res.onAborted(() => {});

  /* collectBody replaces the hand rolled onData accumulation: one callback with
   * the whole body, or null once the body grew past maxBodySize. */
  res.collectBody(maxBodySize, (body) => {
    if (body === null) {
      /* Too large; res.close calls onAborted */
      res.close();
      return;
    }
    let obj;
    try {
      obj = JSON.parse(Buffer.from(body));
    } catch (e) {
      /* Not JSON; res.close calls onAborted */
      res.close();
      return;
    }
    res.writeHeader('content-type', 'application/json').end(JSON.stringify(obj));
  });
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
