const uWS = require('../dist/uws.js');

const port = 3000;

const world = new (require('util').TextEncoder)().encode('World!');

uWS.App().get('/hello', (res, req) => {
  res.end(world);
}).get('/*', (res, req) => {
  res.writeHeader('content-type', 'text/html; charset= utf-8').end(req.getHeader('user-agent') + ' är din user agent, biatch!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

console.log('Timers will not work until us_loop_integrate is done');
