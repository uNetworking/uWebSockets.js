/* This is a script not for Node.js but runs completely stand-alone using loopery */
print('Welcome to loopery!');

const port = 3000;

App().get('/hello', (res, req) => {
  res.end('world!');
}).get('/*', (res, req) => {
  res.writeHeader('content-type', 'text/html; charset= utf-8').end(req.getHeader('user-agent') + ' är din user agent, biatch!');
}).listen(port, (token) => {
  if (token) {
    print('Listening to port ' + port);
  } else {
    print('Failed to listen to port ' + port);
  }
});
