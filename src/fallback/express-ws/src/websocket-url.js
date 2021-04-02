const trailingSlash = require('./trailing-slash');

/* The following fixes HenningM/express-ws#17, correctly. */
module.exports = function websocketUrl(url) {
  if (url.indexOf('?') !== -1) {
    const [baseUrl, query] = url.split('?');

    return `${trailingSlash(baseUrl)}.websocket?${query}`;
  }
  return `${trailingSlash(url)}.websocket`;
}
