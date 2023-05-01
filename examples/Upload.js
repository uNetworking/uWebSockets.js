/* Minimal POST example a SSL/non-SSL */

const uWS = require('../dist/uws.js');
const port = 9001;
const fs = require('fs');

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/', (res, req) => {
  res.end(`
      <html lang="en"><body>
          <form method="post" enctype="multipart/form-data">
            <div>
              <label for="file">Choose files to upload</label>
              <input type="file" id="file" name="file" multiple />
            </div>
            <div>
              <button>Submit</button>
            </div>
          </form>
      </body></html>`
  )
}).get('/streamBigFile', (res, req) => {
  res.end(`
      <html lang="en"><body>
          <input id="file" type="file"><button onclick="upload()">UPLOAD</button>
          <script>
              function upload() {
                  const [file] = document.getElementById('file').files;
                  fetch('streamBigFile?fileName='+file.name, {
                      method : 'POST',
                      body: file
                  });
              }
          </script>
      </body></html>`)
}).post('/*', (res, req) => {

  const header = req.getHeader('content-type');
  let buffer = Buffer.from('');
  res.onData((ab, isLast) => {
    buffer = Buffer.concat([buffer, Buffer.from(ab)]);
    if (isLast) {
      for (const {name, filename, data} of uWS.getParts(buffer, header)) {
        if (name === 'file' && filename) {
          fs.writeFileSync(filename, Buffer.from(data));
        }
      }
      end(res);
    }
  });

    abort(res);
}).post('/streamBigFile', (res, req) => {

  const fileName = req.getQuery("fileName");
  const writeStream = fs.createWriteStream(fileName);

  res.onData((chunk, isLast) => {
    /* Buffer this anywhere you want to */
    console.log('Got chunk of data with length ' + chunk.byteLength + ', isLast: ' + isLast);
    writeStream.write(Buffer.from(chunk.slice(0)));
    /* We respond when we are done */
    if (isLast) {
      writeStream.close();
      end(res);
    }
  });

    abort(res, writeStream);
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

function abort(res, writeStream) {
  res.onAborted(() => {
    /* Request was prematurely aborted, stop reading */
    writeStream?.close();
    console.log('Eh, okay. Thanks for nothing!');
  });
}

function end(res) {
  res.end('Thanks for the data!');
}