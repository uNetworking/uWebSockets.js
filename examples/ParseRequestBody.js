/* This is an example of parsing request body */

const uWS = require('../dist/uws.js');
const port = 9001;

/* Helper function to parse JSON body */
const parseJSONBody = (res, callback) => {
    let buffer;
    /* Register onData callback */
    res.onData((ab, isLast) => {
        const chunk = Buffer.from(ab);
        if (isLast) {
            let json;
            try {
                json = JSON.parse(buffer ? Buffer.concat([buffer, chunk]) : chunk);
            } catch (e) {
                json = undefined;
            }
            callback(json);
        } else if (buffer) {
            buffer = Buffer.concat([buffer, chunk]);
        } else {
            buffer = Buffer.concat([chunk]);
        }
    });
};

const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/jsonAPI', (res, req) => {
    /* Attach onAborted handler because body parsing is async */
    res.onAborted(() => {
        console.log('Request aborted!');
    });
    parseJSONBody(res, (parsedJson) => {
        if (parsedJson === undefined) {
            console.log('Invalid JSON or no data at all!');
            res.writeStatus('400 Bad Request').end();
            return;
        }
        console.log('Valid JSON:', parsedJson);
        /* We are already corked from onData callback */
        res.end('Thanks for your data!');
    });
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});
