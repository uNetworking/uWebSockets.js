const TemplatedApp = require("./templated-app");

function App(options) {
  return new TemplatedApp(options);
}

function us_listen_socket_close(listenSocket) {
  listenSocket.close();
}

// TODO: not implementing this yet as it's not
// currently used by ganache
function getParts(body, contentType) {
  return undefined;
}

const DISABLED = 0;
const SHARED_COMPRESSOR = 1;
const DEDICATED_COMPRESSOR_3KB = 9 << 8 | 1;
const DEDICATED_COMPRESSOR_4KB = 9 << 8 | 2;
const DEDICATED_COMPRESSOR_8KB = 10 << 8 | 3;
const DEDICATED_COMPRESSOR_16KB = 11 << 8 | 4;
const DEDICATED_COMPRESSOR_32KB = 12 << 8 | 5;
const DEDICATED_COMPRESSOR_64KB = 13 << 8 | 6;
const DEDICATED_COMPRESSOR_128KB = 14 << 8 | 7;
const DEDICATED_COMPRESSOR_256KB = 15 << 8 | 8;

module.exports = {
  App,
  us_listen_socket_close,
  getParts,
  DISABLED,
  SHARED_COMPRESSOR,
  DEDICATED_COMPRESSOR_3KB,
  DEDICATED_COMPRESSOR_4KB,
  DEDICATED_COMPRESSOR_8KB,
  DEDICATED_COMPRESSOR_16KB,
  DEDICATED_COMPRESSOR_32KB,
  DEDICATED_COMPRESSOR_64KB,
  DEDICATED_COMPRESSOR_128KB,
  DEDICATED_COMPRESSOR_256KB
};
