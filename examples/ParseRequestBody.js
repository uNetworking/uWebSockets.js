/* This is an example of parsing request body (JSON / URL-encoded form) */

const uWS = require('../dist/uws.js');
const querystring = require('node:querystring');
const port = 9001;

/* Helper function to parse request body */
const parseRequestBody = (res, callback) => {
    let buffer = Buffer.alloc(0);
    /* Register data callback */
    res.onData((ab, isLast) => {
        /* Concat every buffer */
        buffer = Buffer.concat([buffer, Buffer.from(ab)]);
        if (isLast) callback(buffer);
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
    parseRequestBody(res, (bodyBuffer) => {
        try {
            const parsedJson = JSON.parse(bodyBuffer.toString());
            console.log('Valid JSON: ');
            console.log(parsedJson);
            res.cork(() => {
                res.end('Thanks for this json!');
            });
        } catch {
            console.log('Invalid JSON or no data at all!');
            res.cork(() => {
                res.writeStatus('400 Bad Request').end();
            });
        }
    });
}).post('/formPost', (res, req) => {
    /* Attach onAborted handler because body parsing is async */
    res.onAborted(() => {
        console.log('Request aborted!');
    });
    parseRequestBody(res, (bodyBuffer) => {
        let formData;
        try { formData = querystring.parse(bodyBuffer.toString()); }
        catch { formData = null; }
        if (formData && formData.myData) {
            console.log('Valid form body: ');
            console.log(formData);
            res.cork(() => {
                res.end('Thanks for your data!');
            });
        } else {
            console.log('Invalid form body or no data at all!');
            res.cork(() => {
                res.end('Invalid form body');
            });
        }
    });
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});
