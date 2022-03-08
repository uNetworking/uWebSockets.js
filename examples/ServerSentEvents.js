/* Server-sent events (EventSource) example */
/* curl -n localhost:9001 # check events by using curl */

const uWS = require('../dist/uws.js');
const port = 9001;

const headers = [
  ['Content-Type', 'text/event-stream'],
  ['Connection', 'keep-alive'],
  ['Cache-Control', 'no-cache']
]

function sendHeaders(res) {
  for (const [header, value] of headers) {
    res.writeHeader(header, value)
  }
}

function serializeData(data) {
  return `data: ${JSON.stringify(data)}\n\n`
}

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/*', (res) => {
  const clientId = Date.now()
  console.log(`Client with id: ${clientId} connected, starting streaming`)
  sendHeaders(res);
  res.writeStatus('200 OK')

  let intervalRef = setInterval(() => {
    res.write(serializeData({ message: 'Hello world!' }))
  }, 1000)

  res.onAborted(() => {
    clearInterval(intervalRef)
    intervalRef = undefined
    console.log(`Client with id: ${clientId} disconnected`)
  })
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
