const uWS = require('uWebSockets.js');
const { v4: uuid } = require('uuid')
const port = 5000;

let SOCKETS = [];
let connectedUsers= [];

const decoder = new TextDecoder('utf-8');

const MESSAGE_ENUM = Object.freeze({
  SELF_CONNECTED: "SELF_CONNECTED",
  CLIENT_CONNECTED: "CLIENT_CONNECTED",
  CLIENT_DISCONNECTED: "CLIENT_DISCONNECTED",
  CLIENT_MESSAGE: "CLIENT_MESSAGE"
});

function getRandomInt() {
    return Math.floor(Math.random() * Math.floor(9999));
}
  
function createName(randomInt) {
    return SOCKETS.find(ws => ws.name === `user-${randomInt}`) ? createName(getRandomInt()) : `user-${randomInt}`
}

const app = uWS.App()
  .ws('/ws',  {
    compression: 0,
    maxPayloadLength: 16 * 1024 * 1024,
    idleTimeout: 60,
  
    open: (ws,req) => {
        console.log(ws);
        ws.id = uuid();
        ws.username = createName(getRandomInt());
      
        ws.subscribe(MESSAGE_ENUM.CLIENT_CONNECTED);
        ws.subscribe(MESSAGE_ENUM.CLIENT_DISCONNECTED);
        ws.subscribe(MESSAGE_ENUM.CLIENT_MESSAGE);

        SOCKETS.push(ws);
        let selfMsg = {
            type: MESSAGE_ENUM.SELF_CONNECTED,
            body: {
              id: ws.id,
              name: ws.username
            }
        }
        ws.send(JSON.stringify(selfMsg));
    },

    message: (ws, message, isBinary) => {

        let clientMsg = JSON.parse(decoder.decode(message));
        let serverMsg = {};
      
        switch (clientMsg.type) {
          case MESSAGE_ENUM.CLIENT_MESSAGE:
            serverMsg = {
              type: MESSAGE_ENUM.CLIENT_MESSAGE,
              sender: ws.username,
              body: clientMsg.body
            };
            console.log("serverMsg is : ", serverMsg);
            ws.publish(MESSAGE_ENUM.CLIENT_MESSAGE, JSON.stringify(serverMsg));
            break;
          default:
            console.log("Unknown message type.");
        }
      },

      close: (ws, code, message) => {
        SOCKETS.find((socket, index) => {
          if (socket && socket.id === ws.id) {
            SOCKETS.splice(index, 1);
          }
        });
      
        let pubMsg = {
          type: MESSAGE_ENUM.CLIENT_DISCONNECTED,
          body: {
            id: ws.id,
            name: ws.username
          }
        }
        ws.publish(MESSAGE_ENUM.CLIENT_DISCONNECTED, JSON.stringify(pubMsg));
      }
  }).listen(port, token => {
    token ?
    console.log(`Listening to port ${port}`) :
    console.log(`Failed to listen to port ${port}`);
  });
