/* Minimal POST example a SSL/non-SSL */

const uWS = require('../dist/uws.js');
const port = 9001;
const fs = require('fs');
const querystring = require("querystring");


const app = uWS
    ./*SSL*/ App({
        key_file_name: "misc/key.pem",
        cert_file_name: "misc/cert.pem",
        passphrase: "1234"
    })
    .get('/', (res, req) => {
        res.end(`
        <html lang="en"><body>
            <input id="file" type="file"><br><br>
            <button onclick="upload()">UPLOAD</button><br><br>
            <script>
                function upload() {
                    fetch('upload?fileName=fileName.yourExtension', {
                        method : 'POST',
                        headers: { "Content-Type": "multipart/form-data;" },
                        body: document.getElementById('file').files[0]
                    })
                }
            </script>
        </body></html>`)
    })
    .post('/upload', async (res, req) => {
        res.writeHeader('Access-Control-Allow-Origin', req.getHeader('origin'))
            .onAborted(() => console.error('UPLOAD ABORTED'));

        const {fileName} = {...querystring.parse(req.getQuery())};
        let writeStream = fs.createWriteStream(fileName);

        await new Promise((resolve, reject) => {
            writeStream.on('error', reject);
            res.onData((chunk, isLast) => {
                /* Buffer this anywhere you want to */
                console.log('Got chunk of data with length', chunk.byteLength, 'isLast:', isLast);
                writeStream.write(Buffer.from(chunk.slice(0)));
                isLast && resolve();
            })
        });

        /* We respond when we are done */
        res.end('Thanks for the data!');
    })
    .listen(port, token => {
        if (token) {
            console.log("Listening to port " + port);
        } else {
            console.log("Failed to listen to port " + port);
        }
    });
