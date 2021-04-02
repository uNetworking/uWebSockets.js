class ResponseWrapper {
  constructor(res, req) {
    this.res = res;
    this.req = req;
  }

  writeStatus(status) {
    const statusCode = parseInt(status, 10);
    if (!Number.isNaN(statusCode)) {
      this.res.status(statusCode);
    }
    return this;
  }

  writeHeader(key, value) {
    this.res.set(key, value);
    return this;
  }

  write(chunk) {
    //
  }

  end(body) {
    if (typeof body !== "undefined") {
      this.res.send(body);
    } else {
      this.res.end();
    }
  }

  // TODO: while this is used by ganache,
  // there isn't a clear method on the
  onAborted(handler) {
    return this;
  }

  onData(cb) {
    let buf;
    if (Buffer.isBuffer(this.req.body)) {
      buf = new Uint8Array(this.req.body).buffer;
    } else {
      buf = new ArrayBuffer(0);
    }
    cb(buf, true);
    return this;
  }

  cork(cb) {
    cb();
  }
}

module.exports = ResponseWrapper;
