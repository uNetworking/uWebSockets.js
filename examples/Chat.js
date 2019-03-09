const uWS = require('../dist/uws.js');
const port = 9001;
const stringDecoder = new (require('string_decoder').StringDecoder)('utf8');
const connected_clients = new Map(); // Workaround until PubSub is available

const app = uWS./*SSL*/App({
  /*key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'*/
}).ws('/io', {
  /* Options */
  compression: 0,
  maxPayloadLength: 16 * 1024 * 1024,
  idleTimeout: 30,
  /* Handlers */
  open: (ws, req) => {
    console.log('New connection:');
    console.log('User-Agent: ' + req.getHeader('user-agent'));
    ws.client = {
      uuid: create_UUID(),
      nickname: ''
    }
    console.log('UUID: ' + ws.client.uuid);
    connected_clients.set(ws.client.uuid, ws);
    // ws.subscribe() not implemented yet.
    //ws.subscribe('#');
  },
  message: (ws, message) => {
    // Convert message from ArrayBuffer to String asuming it is UTF8
    message = stringDecoder.write(new DataView(message));
    if (message === '') { // PING
      ws.send(''); // PONG
    }
    else {
      handleMessage(ws, message);
    }
  },
  drain: (ws) => {
    console.log('WebSocket backpressure: ' + ws.getBufferedAmount());
  },
  close: (ws, code, message) => {
    console.log('WebSocket closed, uuid: ' + ws.client.uuid);
    connected_clients.delete(ws.client.uuid);
    publish(ws, 'info', 'user left', {
      nickname: ws.client.nickname,
      participants: connected_clients.size
    });
  }
}).any('/*', (res, req) => {
  res.end('Nothing to see here!');
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

function handleMessage(ws, message) {
  console.log('<- ' + ws.client.uuid + ': ' + message);
  var indexOfComma = message.indexOf(',');
  var command = message,
    payload;
  if (indexOfComma >= 0) {
    command = message.slice(0, indexOfComma);
    payload = JSON.parse(message.slice(indexOfComma + 1));
  }
  switch (command) {
    case 'login':
      ws.client.nickname = payload.nickname;
      send(ws, 'welcome', {
        uuid: ws.client.uuid
      });
      publish(ws, 'info', 'user joined', {
        nickname: ws.client.nickname,
        participants: connected_clients.size
      });
      break;
    case 'new message':
      publish(ws, 'room', 'new message', {
        nickname: ws.client.nickname,
        uuid: ws.client.uuid,
        text: payload.text
      });
      break;
  }
}

function send(ws, command, payload) {
  ws.send(command + ',' + JSON.stringify(payload));
}

function publish(ws, topic, command, payload) {
  // ws.publish() not implemented yet. Using Map.forEach untill PubSub is available 
  //ws.publish(topic, command + ',' + JSON.stringify(payload));
  connected_clients.forEach(client_ws => {
    client_ws.send(command + ',' + JSON.stringify(payload));
  })
}

// UUID generator from: https://gist.github.com/LeverOne/1308368
function create_UUID(a, b) { for (b = a = ''; a++ < 36; b += a * 51 & 52 ? (a ^ 15 ? 8 ^ Math.random() * (a ^ 20 ? 16 : 4) : 4).toString(16) : '-'); return b }
