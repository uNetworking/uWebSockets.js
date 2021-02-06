/* Minimal POST example a SSL/non-SSL */
/* for multipart/form-data encoding, this is not a perfect parsing but an example */

const uWS = require('../dist/uws.js');
const port = 9001;
const fs = require('fs');

const app = uWS./*SSL*/App({
    key_file_name: 'misc/key.pem',
    cert_file_name: 'misc/cert.pem',
    passphrase: '1234'
}).get('/*', (res, req) => {
    res.end('<form method="post" enctype="multipart/form-data">' +
        ' <div>' +
        '   <label for="file">Select the file to send</label>' +
        '   <input type="file" id="file" name="file">' +
        ' </div>' +
        ' <div>' +
        '   <button>Send</button>' +
        ' </div>' +
        '</form>');
}).post('/*', (res, req) => {
    let writeStream, fileName;
    console.log('Posted to', req.getUrl());
    res.onData((chunk, isLast) => {
        /* Buffer this anywhere you want to */
        console.log('Got chunk of data with length', chunk.byteLength, 'isLast:', isLast);
        if (chunk.byteLength) {
            let buffer = Buffer.from(chunk);
            const content = buffer.toString();
            if (!writeStream && content.startsWith('--------------------------')) {
                const split = content.split('\r\n');
                // Content-Disposition: form-data; name="data"; filename="example.png"
                fileName = split[1].split('filename="')[1].split('"')[0];
                // sometimes data are directly provided into the first chunk after the headers and an empty string
                /*
                [
                      '--------------------------af358f6352372157',
                      'Content-Disposition: form-data; name="data"; filename="toast.png"',
                      'Content-Type: application/octet-stream',
                      '',
                      '',
                      'BinaryHere'
                    ]
                 */
                let offset = isLast ? split.length : split.length + 1; // to count the split \r\n
                for (let i = 0; i < split.length; i++) {
                    if (split[i].length === 0) {
                        if (!isLast) offset++;
                        for (let y = i + 1; y < split.length; y++) {
                            if (split[y].length > 0) {
                                if (!writeStream) {
                                    writeStream = fs.createWriteStream('upload' + fileName, {flags: 'w'});
                                }
                                for (let z = 0; y === i + 1 && z < y; z++) offset += split[z].length;
                            }
                        }
                        if (writeStream) {
                            writeStream.write(
                                new Uint8Array(chunk.slice(0), offset, chunk.byteLength - offset - (isLast ? 52 : 0))
                            );
                        }
                        break;
                    }
                }
                if (!writeStream) writeStream = fs.createWriteStream('upload' + fileName, {flags: 'w'});
            } else {
                writeStream.write(
                    new Uint8Array(
                        chunk.slice(0),
                        0,
                        isLast ? chunk.byteLength - 52 : undefined // \r\n--------------------------267de2f701d515a1--\r\n
                    )
                );
            }
        }
        /* We respond when we are done */
        if (isLast) {
            if (writeStream) writeStream.close();
            res.end('Thanks for the data!');
        }
    });

    res.onAborted(() => {
        /* Request was prematurely aborted, stop reading */
        if (writeStream) writeStream.close();
        console.log('Eh, okay. Thanks for nothing!');
    });
}).listen(port, (token) => {
    if (token) {
        console.log('Listening to port ' + port);
    } else {
        console.log('Failed to listen to port ' + port);
    }
});
