/* The two entries of the package: index.js for require and index.mjs for import, generated from it.
 * Both must give the same exports, and a server started from each must answer. */
import { createRequire } from 'module';
import http from 'http';
import * as esm from '../dist/index.mjs';

const cjs = createRequire(import.meta.url)('../dist/index.js');
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
function get(port) {
    return new Promise((resolve, reject) => {
        http.get({ host: '127.0.0.1', port, path: '/', agent: false }, (response) => {
            const chunks = [];
            response.on('data', (chunk) => chunks.push(chunk));
            response.on('end', () => resolve(Buffer.concat(chunks).toString()));
        }).on('error', reject);
    });
}

function serve(uWS, name) {
    return new Promise((resolve, reject) => {
        uWS.App().get('/*', (res) => {
            res.end(`hello from ${name}`);
        }).listen('127.0.0.1', 0, (token) => {
            if (!token) return reject(new Error(`${name}: listen failed`));
            get(uWS.us_socket_local_port(token)).then((body) => {
                uWS.us_listen_socket_close(token);
                resolve(body);
            }, reject);
        });
    });
}

const keys = Object.keys(cjs).sort();
const named = Object.keys(esm).filter((key) => key !== 'default').sort();
check('default export is the CommonJS module', esm.default === cjs, true);
check('named exports are the CommonJS exports', named.join(), keys.join());
check('named exports have the CommonJS values', keys.filter((key) => esm[key] !== cjs[key]).join(), '');

check('require: a server answers', await serve(cjs, 'require'), 'hello from require');
check('import: a server answers', await serve(esm, 'import'), 'hello from import');

if (failures) {
    console.error(`\n${failures} ESM test(s) failed`);
    process.exit(1);
}
console.log('\nAll ESM tests passed');
process.exit(0);
