import {
  RecognizedString,
  WebSocket as uWsWebSocket,
  WebSocketBehavior
} from "../../docs/index";
import InternalWebSocket from "ws";

export class WebSocket implements uWsWebSocket {
  private internalWs: InternalWebSocket;

  constructor(internalWs: InternalWebSocket) {
    this.internalWs = internalWs;
  }

  initialize(behavior: WebSocketBehavior) {
    this.internalWs.removeAllListeners();

    if (typeof behavior.open === "function") {
      behavior.open(this);
    }

    this.internalWs.on("error", (error) => {
      // if max payload size is exceed we want to match uWS error handling.
      // It propagates the error with code `1006` and message "Received too big
      // message".
      if (error.message === "Max payload size exceeded") {
        (this.internalWs as any)._closeCode = 1006;
        (this.internalWs as any)._closeMessage = "Received too big message";
      } else {
        throw error;
      }
    });

    this.internalWs.on("message", (message) => {
      if (typeof behavior.message === "function") {
        if (typeof message === "string") {
          const buf = new ArrayBuffer(message.length);
          const bufView = new Uint8Array(buf);
          for (let i = 0; i < message.length; i++) {
            bufView[i] = message.charCodeAt(i);
          }
          behavior.message(this, buf, false);
        } else if (Buffer.isBuffer(message)) {
          const buf = (new Uint8Array(message)).buffer;
          behavior.message(this, buf, true);
        } else if (Array.isArray(message)) {
          // array of buffers. do nothing?
        } else {
          behavior.message(this, message, true);
        }
      }
    });

    // TODO: there is no "drain" event for `ws`
    // this currently isn't used by ganache so moving along

    this.internalWs.on("close", (code, reason) => {
      if (typeof behavior.close === "function") {
        const buf = new ArrayBuffer(reason.length);
        const bufView = new Uint8Array(buf);
        for (let i = 0; i < reason.length; i++) {
          bufView[i] = reason.charCodeAt(i);
        }
        behavior.close(this, code, buf);
      }

      this.internalWs.removeAllListeners(); // may be redundant
    });

    this.internalWs.on("ping", data => {
      if (typeof behavior.ping === "function") {
        behavior.ping(this);
      }
    });

    this.internalWs.on("pong", data => {
      if (typeof behavior.pong === "function") {
        behavior.pong(this);
      }
    });
  }

  send(message: RecognizedString, isBinary: boolean, compress: false) {
    this.internalWs.send(message, {
      binary: isBinary,
      compress,
    });
    return true;
  }

  getBufferedAmount() {
    return this.internalWs.bufferedAmount;
  }

  end(code: number, shortMessage: RecognizedString) {
    this.internalWs.close(code, shortMessage.toString());
    return this;
  }

  close() {
    this.internalWs.terminate();
    this.internalWs.removeAllListeners(); // may be redundant
    return this;
  }

  ping(message: RecognizedString) {
    this.internalWs.ping(message);
    return true;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  subscribe(topic: RecognizedString) {
    return this;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  unsubscribe(topic: RecognizedString) {
    return false;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  unsubscribeAll() {
    return;
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  publish(topic: RecognizedString, message: RecognizedString, isBinary: boolean, compress: boolean) {
    return this;
  }

  cork(cb: () => void) {
    cb();
  }

  // TODO this isn't currently necessary
  // so we're not implementing it yet
  getRemoteAddress() {
    return new ArrayBuffer(0);
  }

  getRemoteAddressAsText() {
    const url = this.internalWs.url;
    const buf = new ArrayBuffer(url.length);
    const bufView = new Uint8Array(buf);
    for (let i = 0; i < url.length; i++) {
      bufView[i] = url.charCodeAt(i);
    }
    return buf;
  }
}
