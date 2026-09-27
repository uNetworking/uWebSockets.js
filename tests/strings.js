/* Strings from JS to C++ and out on the wire, compared byte for byte. A one-byte V8 string is
 * Latin-1, not utf-8: #1262 mixed the two and #1280 got "str<?>ngar" back. */
const uWS = require('../dist/uws.js');
const http = require('http');

const port = 9002;
let failures = 0;

function check(what, actual, expected) {
    if (actual !== expected) {
        failures++;
        console.error(`Test failed: ${what}\n  expected ${JSON.stringify(expected)}\n  actual   ${JSON.stringify(actual)}`);
    } else {
        console.log(`Test passed: ${what}`);
    }
}

/* no keep-alive: a client socket still open at process.exit trips a libuv assert on Windows */
function get(path, headers) {
    return new Promise((resolve, reject) => {
        http.get({ host: '127.0.0.1', port, path, headers, agent: false }, (response) => {
            const chunks = [];
            response.on('data', (chunk) => chunks.push(chunk));
            response.on('end', () => resolve({ headers: response.headers, body: Buffer.concat(chunks) }));
        }).on('error', reject);
    });
}

/* V8 keeps the first five as one-byte strings, and only ASCII is the same bytes in Latin-1 and utf-8 */
const CASES = {
    'ascii': 'the quick brown fox',
    'latin1': 'Har dina strängar gått av?',
    'latin1 edge': 'ÿ',
    'empty': '',
    'nul inside': 'a\u0000b',
    'bmp': '中文テスト',
    'astral': 'a\u{1f600}b\u{1f4a9}c',
    'mixed': 'aä中\u{1f600}z',
    /* past the 128 KB pool in NativeString, so the copy takes the malloc fallback */
    'past the pool, ascii': 'x'.repeat(200 * 1024),
    'past the pool, latin1': 'x'.repeat(200 * 1024) + 'é'
};

const app = uWS.App();

app.get('/body/:name', (res, req) => {
    const value = CASES[decodeURIComponent(req.getParameter(0))];
    res.writeHeader('content-type', 'text/plain; charset=utf-8');
    res.end(value);
});

app.get('/header/:name', (res, req) => {
    res.writeHeader('x-echo', CASES[decodeURIComponent(req.getParameter(0))]);
    res.end('ok');
});

/* the lookup key of getHeader crosses the same way */
app.get('/read', (res, req) => {
    res.end(req.getHeader('x-ascii'));
});

app.listen(port, async (token) => {
    if (!token) {
        console.error('Test failed: could not listen on ' + port);
        process.exit(1);
    }

    /* bytes, not decoded text: a decoder can hide a wrong encoding behind a replacement char */
    for (const [name, value] of Object.entries(CASES)) {
        const { body } = await get(`/body/${encodeURIComponent(name)}`);
        check(`body bytes, ${name}`, body.toString('hex'), Buffer.from(value, 'utf8').toString('hex'));
    }

    /* read back as Latin-1, which keeps the bytes that were on the wire */
    for (const name of ['ascii', 'latin1', 'latin1 edge', 'bmp', 'astral', 'mixed']) {
        const { headers } = await get(`/header/${encodeURIComponent(name)}`);
        const echoed = headers['x-echo'];
        check(`header value, ${name}`, Buffer.from(echoed, 'latin1').toString('hex'), Buffer.from(CASES[name], 'utf8').toString('hex'));
    }

    const { body } = await get('/read', { 'x-ascii': 'plain' });
    check('getHeader key', body.toString(), 'plain');

    if (failures) {
        console.error(`\n${failures} string test(s) failed`);
        process.exit(1);
    }
    console.log('\nAll string tests passed');
    process.exit(0);
});
