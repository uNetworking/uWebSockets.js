/* Minimal POST example a SSL/non-SSL */
/* todo: proper throttling, better example */

/* a great example would be a little parser
 * to handle multiple files in one request */

const uWS = require('../dist/uws.js');
const port = 9001;
const fs = require('fs');
const querystring = require("querystring");

const app = uWS./*SSL*/App({
  key_file_name: 'misc/key.pem',
  cert_file_name: 'misc/cert.pem',
  passphrase: '1234'
}).get('/', (res, req) => {
    res.end(`
      <html lang="en"><body>
          <input id="file" type="file"><button onclick="upload()">UPLOAD</button>
          <script>
              function upload() {
                  const [file] = document.getElementById('file').files;
                  fetch('upload?fileName='+file.name, {
                      method : 'POST',
                      body: file
                  });
              }
          </script>
      </body></html>`)
  }).post('/*', (res, req) => {
  console.log('Posted to ' + req.getUrl());
  const {fileName} = querystring.parse(req.getQuery());
  const writeStream = fs.createWriteStream(fileName);

  res.onData((chunk, isLast) => {
    /* Buffer this anywhere you want to */
    console.log('Got chunk of data with length ' + chunk.byteLength + ', isLast: ' + isLast);
    writeStream.write(Buffer.from(chunk.slice(0)));
    /* We respond when we are done */
    if (isLast) {
      writeStream.close();
      res.end('Thanks for the data!');
    }
  });

  res.onAborted(() => {
    /* Request was prematurely aborted, stop reading */
    writeStream.close();
    console.log('Eh, okay. Thanks for nothing!');
  });
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});
