import {
  RecognizedString,
  WebSocket as uWsWebSocket,
  WebSocketBehavior,
} from "../../docs/index";
import InternalWebSocket from "ws";

const TOO_BIG_MESSAGE = Buffer.from("Received too big message", "utf8");

/**
 * Converts a buffer to an `ArrayBuffer`.
 *
 * @param {Buffer} buf The buffer to convert
 * @return {ArrayBuffer} Converted buffer
 * @public
 */
function toArrayBuffer(buf: Buffer) {
  if (buf.byteLength === buf.buffer.byteLength) {
    return buf.buffer;
  }

  return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
}

const NODE_VERSION = parseInt(process.versions.node, 10);

export class WebSocket implements uWsWebSocket {
  private internalWs: InternalWebSocket;

  constructor(internalWs: InternalWebSocket) {
    this.internalWs = internalWs;
    // `nodebuffer` is already the default, but I just wanted to be explicit
    // here because when `nodebuffer` is the binaryType the `message` event's
    // data type is guaranteed to be a `Buffer`. We don't need to check for
    // different types of data.
    // I mention all this because if `arraybuffer` or `fragment` is used for the
    // binaryType the `"message"` event's `data` may end up being
    // `ArrayBuffer | Buffer`, or `Buffer[] | Buffer`, respectively.
    internalWs.binaryType = "nodebuffer";
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
        (this.internalWs as any)._closeMessage = TOO_BIG_MESSAGE;

        // node 14 and up automatically destroy the socket after an error occurs
        // but versions before that they don't. We need to do our own cleanup
        // here, otherwise errors (like "Max payload size exceeded") won't
        // immediately close the websocket connection.
        // See https://github.com/websockets/ws/issues/1940#issuecomment-907432872
        // for more information
        if (NODE_VERSION < 14) {
          this.internalWs.terminate();
        }
      } else {
        throw error;
      }
    });

    this.internalWs.on("message", (message: Buffer, isBinary: boolean) => {
      if (typeof behavior.message === "function") {
        behavior.message(this, toArrayBuffer(message), isBinary);
      }
    });

    // TODO: there is no "drain" event for `ws`
    // this currently isn't used by ganache so moving along

    this.internalWs.on("close", (code, reason: Buffer) => {
      if (typeof behavior.close === "function") {
        const buf = reason ? toArrayBuffer(reason) : new ArrayBuffer(0);
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
