import { IncomingMessage, ServerResponse } from "http";
import {
  RecognizedString,
  HttpResponse as uWsHttpResponse,
  us_socket_context_t
} from "../../docs/index";

export class HttpResponse implements uWsHttpResponse {
  response: ServerResponse;
  abortHandler?: () => void;
  dataHandler?: (chunk: ArrayBuffer, isLast: boolean) => void;

  constructor(res: ServerResponse, req: IncomingMessage) {
    this.response = res;

    this.response.on("close", () => {
      this.abortHandler && this.abortHandler();
    });
    req.on("data", data => {
      this.dataHandler && this.dataHandler(data, false);
    });
    req.on("end", () => {
      this.dataHandler && this.dataHandler(Buffer.allocUnsafe(0), true);
    });
  }

  writeContinue() {}

  cork(handler: () => void) {
    if (this.response.cork) {
      this.response.cork();
    }
    handler();
    if (this.response.uncork) {
      this.response.uncork();
    }
  }

  onWritable(handler: (offset: number) => boolean) {
    throw new Error("Not implemented");
    return this;
  }

  onAborted(handler: () => void) {
    this.abortHandler = handler;
    return this;
  }

  onData(handler: (chunk: ArrayBuffer, isLast: boolean) => void) {
    this.dataHandler = handler;
    return this;
  }

  getRemoteAddress(): ArrayBuffer {
    throw new Error("Not implemented");
  }

  getRemoteAddressAsText(): ArrayBuffer {
    throw new Error("Not implemented");
  }

  getProxiedRemoteAddress(): ArrayBuffer {
    throw new Error("Not implemented");
  }

  getProxiedRemoteAddressAsText(): ArrayBuffer {
    throw new Error("Not implemented");
  }

  upgrade<T>(
    userData: T,
    secWebSocketKey: RecognizedString,
    secWebSocketProtocol: RecognizedString,
    secWebSocketExtensions: RecognizedString,
    context: us_socket_context_t
  ) {
    throw new Error("Not implemented");
  }

  write(chunk: RecognizedString) {
    this.response.write(chunk);
    return this;
  }

  tryEnd(
    fullBodyOrChunk: RecognizedString,
    totalSize: number
  ): [boolean, boolean] {
    throw new Error("Not implemented");
  }

  close() {
    return this;
  }

  getWriteOffset(): number {
    throw new Error("Not implemented");
  }

  writeStatus(status: RecognizedString) {
    this.response.statusCode = parseInt(status.toString());
    return this;
  }

  writeHeader(key: RecognizedString, value: RecognizedString) {
    this.response.setHeader(key.toString(), value.toString());
    return this;
  }

  end(data: RecognizedString = "", closeConnection: boolean = false) {
    this.response.end(data);
    return this;
  }
}
