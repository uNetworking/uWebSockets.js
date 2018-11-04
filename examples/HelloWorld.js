/* The stand-alone runtime has uWS namespace already loaded. */
var uWS = uWS ? uWS : require('../dist/uws.js');

const world = 'Strings are slower than ArrayBuffer but who cares for demo purose!';
const port = 3000;

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

/* This is not true for stand-alone */
console.log('Timers will not work until us_loop_integrate is done');
