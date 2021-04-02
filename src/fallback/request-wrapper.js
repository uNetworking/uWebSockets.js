class RequestWrapper {
  constructor(req) {
    this.req = req;
  }

  getHeader(lowerCaseKey) {
    return this.req.get(lowerCaseKey);
  }

  getParameter(index) {
    return this.req.params[index];
  }

  getUrl() {
    return this.req.originalUrl;
  }

  getMethod() {
    return this.req.method;
  }

  getQuery() {
    const idx = this.req.originalUrl.indexOf("?");
    return this.req.originalUrl.substr(idx + 1);
  }

  getQuery(key) {
    // TODO may break for subqueries; i.e. ?foo[bar]=hello
    return this.req.query[key];
  }

  forEach(cb) {
    for (const [key, value] of Object.entries(this.req.headers)) {
      cb(key, value);
    }
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  setYield(doYield) {
    return null;
  }
}

module.exports = RequestWrapper;
