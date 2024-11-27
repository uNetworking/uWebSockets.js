// Simple example of parsing JSON body or URL-encoded form

const querystring = require('node:querystring');
const uWS = require('../dist/uws.js');
const port = 9001;

// Helper function for parsing JSON body
const parseJSONBody = (res, callback) => {
    let buffer = Buffer.alloc(0);
    // Register data callback
    res.onData((ab, isLast) => {
        buffer = Buffer.concat([buffer, Buffer.from(ab)]);
        if (isLast) {
            let parsedJson;
            try { parsedJson = JSON.parse(buffer.toString()); }
            catch { parsedJson = null; }
            callback(parsedJson);
        }
    });
};

// Helper function for parsing URL-encoded form body
const parseFormBody = (res, callback) => {
    let buffer = Buffer.alloc(0);
    // Register data callback
    res.onData((ab, isLast) => {
        buffer = Buffer.concat([buffer, Buffer.from(ab)]);
        if (isLast) {
            let parsedForm;
            try { parsedForm = querystring.parse(buffer.toString()); }
            catch { parsedForm = null; }
            callback(parsedForm);
        }
    });
};

const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/jsonAPI', (res, req) => {
    // Attach onAborted handler because body parsing is async
    res.onAborted(() => {
        res.aborted = true;
    });

    parseJSONBody(res, (object) => {
        if (res.aborted) return;
        if (!object) {
            console.log('Invalid JSON or no data at all!');
            res.cork(() => { // Cork because async
                res.writeStatus('400 Bad Request').end();
            });
        } else {
            console.log('Valid JSON: ');
            console.log(object);
            res.cork(() => {
                res.end('Thanks for this json!');
            });
        }
    });
}).post('/formPost', (res, req) => {
    // Attach onAborted handler because body parsing is async
    res.onAborted(() => {
        res.aborted = true;
    });

    parseFormBody(res, (form) => {
        if (res.aborted) return;
        if (!form || !form.myData) {
            console.log('Invalid form body or no data at all!');
            res.cork(() => { // Cork because async
                res.end('Invalid form body');
            });
        } else {
            console.log('Valid form body: ');
            console.log(form);
            res.cork(() => {
                res.end('Thanks for your data!');
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
