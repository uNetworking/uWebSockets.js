const http = require("http");
const express = require("express");
const expressWs = require("./express-ws");
const WebSocketWrapper = require("./websocket-wrapper");
const RequestWrapper = require("./request-wrapper");
const ResponseWrapper = require("./response-wrapper");
const raw = require("body-parser").raw;

class TemplatedApp {
  constructor(options) {
    // options are ignored as they're for ssl

    this.expressApp = express();
    this.httpServer = http.createServer(this.expressApp);
    this.expressApp.use(raw({
      type: "application/json"
    }));
    this.wsServer = null;
  }

  /** Listens to port and sets Listen Options. Callback hands either false or a listen socket. */
  listen(hostOrPort, portOptionsOrCb, cb) {
    let host;
    let port;
    let options;
    if (typeof hostOrPort === "number") {
      port = hostOrPort;
      if (typeof portOptionsOrCb === "function") {
        cb = portOptionsOrCb;
      } else {
        options = portOptionsOrCb;
      }
    } else {
      host = hostOrPort;
      port = portOptionsOrCb;
    }

    if (!host) {
      host = "127.0.0.1";
    }

    // options can be 0 to do the default or 1 to not share the port
    // TODO: we're going to ignore options for now

    try {
      this.httpServer.listen(port, host, () => {
        if (cb && typeof cb === "function") {
          cb(this.httpServer);
        }
      });
    } catch(e) {
      throw e;
    }

    return this;
  }

  /** Registers an HTTP GET handler matching specified URL pattern. */
  get(pattern, handler) {
    this.expressApp.get(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP POST handler matching specified URL pattern. */
  post(pattern, handler) {
    this.expressApp.post(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP OPTIONS handler matching specified URL pattern. */
  options(pattern, handler) {
    this.expressApp.options(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP DELETE handler matching specified URL pattern. */
  del(pattern, handler) {
    this.expressApp.delete(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP PATCH handler matching specified URL pattern. */
  patch(pattern, handler) {
    this.expressApp.patch(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP PUT handler matching specified URL pattern. */
  put(pattern, handler) {
    this.expressApp.put(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP HEAD handler matching specified URL pattern. */
  head(pattern, handler) {
    this.expressApp.head(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP CONNECT handler matching specified URL pattern. */
  connect(pattern, handler) {
    this.expressApp.connect(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP TRACE handler matching specified URL pattern. */
  trace(pattern, handler) {
    this.expressApp.trace(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers an HTTP handler matching specified URL pattern on any HTTP method. */
  any(pattern, handler) {
    this.expressApp.all(pattern, (req, res) => {
      return handler(new ResponseWrapper(res, req), new RequestWrapper(req));
    });
    return this;
  }

  /** Registers a handler matching specified URL pattern where WebSocket upgrade requests are caught. */
  ws(pattern, behavior) {
    if (typeof this.expressApp.ws !== "function") {
      expressWs(this.expressApp, this.httpServer);
    }

    this.expressApp.ws(pattern, (ws, req) => {
      const wrappedWebsocket = new WebSocketWrapper(ws);

      if (typeof behavior.open === "function") {
        behavior.open(wrappedWebsocket);
      }

      ws.on("upgrade", (req) => {
        if (typeof behavior.upgrade === "function") {
          behavior.upgrade(null, req, {});
        }
      });

      ws.on("message", message => {
        if (typeof behavior.message === "function") {
          if (typeof message === "string") {
            const buf = new ArrayBuffer(message.length);
            const bufView = new Uint8Array(buf);
            for (let i = 0; i < message.length; i++) {
              bufView[i] = message.charCodeAt(i);
            }
            behavior.message(wrappedWebsocket, buf, false);
          } else if (Buffer.isBuffer(message)) {
            const buf = (new Uint8Array(message)).buffer;
            behavior.message(wrappedWebsocket, buf, true);
          } else if (Array.isArray(message)) {
            // array of buffers. do nothing?
          } else {
            behavior.message(wrappedWebsocket, message, true);
          }
        }
      });

      // there is no "drain" event for `ws`
      // this currently isn't used by ganache so moving along

      ws.on("close", (code, reason) => {
        if (typeof behavior.close === "function") {
          const buf = new ArrayBuffer(reason.length);
          const bufView = new Uint8Array(buf);
          for (let i = 0; i < reason.length; i++) {
            bufView[i] = reason.charCodeAt(i);
          }
          behavior.close(wrappedWebsocket, code, buf);
        }
      });

      ws.on("ping", data => {
        if (typeof behavior.ping === "function") {
          behavior.ping(wrappedWebsocket);
        }
      });

      ws.on("pong", data => {
        if (typeof behavior.pong === "function") {
          behavior.pong(wrappedWebsocket);
        }
      });
    });

    return this;
  }

  /** Publishes a message under topic, for all WebSockets under this app. See WebSocket.publish. */
  // TODO this isn't currently necessary
  // so we're not implementing it yet
  publish(topic, message, isBinary, compress) {
    return this;
  }

}

module.exports = TemplatedApp;
