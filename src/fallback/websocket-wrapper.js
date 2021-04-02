class WebSocketWrapper {
  constructor(ws) {
    this.ws = ws;
  }

  send(message, isBinary, compress) {
    this.ws.send(message, {
      binary: isBinary,
      compress,
    });
    return true;
  }

  getBufferedAmount() {
    return this.ws.bufferedAmount;
  }

  end(code, shortMessage) {
    this.ws.close(code, shortMessage);
    return this;
  }

  close() {
    this.ws.terminate();
    return this;
  }

  ping(message) {
    this.ws.ping(message);
    return true;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  subscribe(topic) {
    return this;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  unsubscribe(topic) {
    return false;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  unsubscribeAll() {
    return;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  publish(topic, message, isBinary, compress) {
    return this;
  }

  cork(cb) {
    cb();
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  getRemoteAddress() {
    return ArrayBuffer(0);
  }

  getRemoteAddressAsText() {
    const url = this.ws.url;
    const buf = new ArrayBuffer(url.length);
    const bufView = new Uint8Array(buf);
    for (let i = 0; i < url.length; i++) {
      bufView[i] = url.charCodeAt(i);
    }
    return buf;
  }
}

module.exports = WebSocketWrapper;
