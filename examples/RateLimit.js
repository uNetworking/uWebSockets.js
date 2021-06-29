/* RateLimit example */

const uWS = require('../dist/uws.js');
const port = 9001;

const RateLimit = (limit, interval) => { let now = 0; const last = Symbol(), count = Symbol(); setInterval(() => ++now, interval); return ws => { if (ws[last] != now) { ws[last] = now; ws[count] = 1 } else { return ++ws[count] > limit } } }
const rateLimit = RateLimit(1, 10000) //  limit is: 1 message per 10 seconds.
const rateLimit2 = RateLimit(10, 2000) // limit per interval (milliseconds)


const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).ws('/*', {
    /* Options */
    compression: uWS.SHARED_COMPRESSOR,
    maxPayloadLength: 16 * 1024 * 1024,
    idleTimeout: 10,
    /* Handlers */
    open: (ws) => {
        console.log('A WebSocket connected!');
    },
    message: async (ws, message, isBinary) => {

        // rateLimit(ws) returns true if over limit
        if (rateLimit(ws)) {
            /*
                Do as you wish, personally, I prefer to inform the client, do Log,
                 and close the connection if over limit happened.
                or you can just ignore its message and just return. 
            */
            const dataToSendToClient = {
                action: 'info',
                data: {
                    message: {
                        english: 'over limit! please slow down.',
                        farsi: 'یواش‌تر'
                    }
                }
            }
            await ws.send(JSON.stringify(dataToSendToClient));
            console.warn('rate limit hit!');
            return ws.end();
        }

        //or maybe different type of message or /endpoint needs different limit
        if (ws.type2 && rateLimit2(ws)) {
            console.warn('type2 is over limit');
            return ws.end();
        }

        // rest of message code goes here ...
    },
    drain: (ws) => {
        console.log('WebSocket backpressure: ' + ws.getBufferedAmount());
    },
    close: (ws, code, message) => {
        console.log('WebSocket closed');
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
