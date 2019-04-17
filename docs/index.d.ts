/* Under construction, lots of things to add */

/** Native type representing a raw uSockets struct us_listen_socket. */
interface us_listen_socket {

}

/** Recognized string types, things C++ can read and understand as strings */
type RecognizedString = string | ArrayBuffer | Uint8Array | Int8Array | Uint16Array | Int16Array | Uint32Array | Int32Array | Float32Array | Float64Array;

/** A WebSocket connection that is valid from open to close event */
interface WebSocket {
    /** Sends a message. Make sure to check getBufferedAmount() before sending. Returns true for success, false for built up backpressure that will drain when time is given. */
    send(message: RecognizedString, isBinary?: boolean, compress?: boolean) : boolean;

    /** Returns the bytes buffered in backpressure. */
    getBufferedAmount() : number;

    /** Gradefully closes this WebSocket. Immediately calls close handler. */
    end(code: number, shortMessage: RecognizedString) : WebSocket;

    /** Forefully closes this WebSocket. Immediately calls close handler. */
    close() : WebSocket;

    /** Subscribe to a topic in MQTT syntax */
    subscribe(topic: RecognizedString) : WebSocket;

    /** Publish a message to a topic in MQTT syntax */
    publish(topic: RecognizedString, message: RecognizedString) : WebSocket;

    /** Unsubscribe from topic (not implemented yet) */
    unsubscribe(topic: RecognizedString) : WebSocket;

    /** Returns the remote IP address */
    getRemoteAddress() : ArrayBuffer;
    
    /** Arbitrary user data may be attached to this object */
    [key: string]: any;
}

/** An HttpResponse is valid until either onAborted callback or any of the .end/.tryEnd calls succeed. You may attach user data to this object. */
interface HttpResponse {
    /** Writes the HTTP status message such as "200 OK". */
    writeStatus(status: RecognizedString) : HttpResponse;
    /** Writes key and value to HTTP response. */
    writeHeader(key: RecognizedString, value: RecognizedString) : HttpResponse;
    /** Enters or continues chunked encoding mode. Writes part of the response. End with zero length write. */
    write(chunk: RecognizedString) : HttpResponse;
    /** Ends this response by copying the contents of body. */
    end(body?: RecognizedString) : HttpResponse;
    /** Ends this response, or tries to, by streaming appropriately sized chunks of body. Use in conjunction with onWritable. Returns tuple [ok, hasResponded].*/
    tryEnd(fullBodyOrChunk: RecognizedString, totalSize: number) : [boolean, boolean];

    /** Immediately force closes the connection. */
    close() : HttpResponse;

    /** Returns the global byte write offset for this response. Use with onWritable. */
    getWriteOffset() : number;

    /** Registers a handler for writable events. Continue failed write attempts in here.
     * You MUST return true for success, false for failure.
     * Writing nothing is always success, so by default you must return true.
     */
    onWritable(handler: (offset: number) => boolean) : HttpResponse;

    /** Every HttpResponse MUST have an attached abort handler IF you do not respond
     * to it immediately inside of the callback. Returning from an Http request handler
     * without attaching (by calling onAborted) an abort handler is ill-use and will termiante.
     * When this event emits, the response has been aborted and may not be used. */
    onAborted(handler: (res: HttpResponse) => void) : HttpResponse;

    /** Handler for reading data from POST and such requests. You MUST copy the data of chunk if isLast is not true. We Neuter ArrayBuffers on return, making it zero length.*/
    onData(handler: (chunk: ArrayBuffer, isLast: boolean) => void) : HttpResponse;

    /** Returns the remote IP address */
    getRemoteAddress() : ArrayBuffer;

    /** Arbitrary user data may be attached to this object */
    [key: string]: any;
}

/** An HttpRequest is stack allocated and only accessible during the callback invocation. */
interface HttpRequest {
    /** Returns the lowercased header value or empty string. */
    getHeader(lowerCaseKey: RecognizedString) : string;
    /** Returns the parsed parameter at index. Corresponds to route. */
    getParameter(index: number) : string;
    /** Returns the URL including initial /slash */
    getUrl() : string;
    /** Returns the HTTP method, useful for "any" routes. */
    getMethod() : string;
    /** Returns the part of URL after ? sign or empty string. */
    getQuery() : string;
    /** Loops over all headers. */
    forEach(cb: (key: string, value: string) => void) : void;
}

/** A structure holding settings and handlers for a WebSocket route handler. */
interface WebSocketBehavior {
    /** Maximum length of received message. */
    maxPayloadLength?: number;
    /** Maximum amount of seconds that may pass without sending or getting a message. */
    idleTimeout?: number;
    /** 0 = no compression, 1 = shared compressor, 2 = dedicated compressor. See C++ project. */
    compression?: CompressOptions;
    /** Handler for new WebSocket connection. WebSocket is valid from open to close, no errors. */
    open?: (ws: WebSocket, req: HttpRequest) => void;
    /** Handler for a WebSocket message. */
    message?: (ws: WebSocket, message: ArrayBuffer, isBinary: boolean) => void;
    /** Handler for when WebSocket backpressure drains. Check ws.getBufferedAmount(). */
    drain?: (ws: WebSocket) => void;
    /** Handler for close event, no matter if error, timeout or graceful close. You may not use WebSocket after this event. */
    close?: (ws: WebSocket, code: number, message: ArrayBuffer) => void;
}

/** Options used when constructing an app. */
interface AppOptions {
    key_file_name?: RecognizedString;
    cert_file_name?: RecognizedString;
    passphrase?: RecognizedString;
    dh_params_file_name?: RecognizedString;
    /** This translates to SSL_MODE_RELEASE_BUFFERS */
    ssl_prefer_low_memory_usage?: boolean;
}

/** TemplatedApp is either an SSL or non-SSL app. */
interface TemplatedApp {
    /** Listens to hostname & port. Callback hands either false or a listen socket. */
    listen(host: RecognizedString, port: number, cb: (listenSocket: us_listen_socket) => void): TemplatedApp;
    /** Listens to port. Callback hands either false or a listen socket. */
    listen(port: number, cb: (listenSocket: any) => void): TemplatedApp;
    /** Registers an HTTP GET handler matching specified URL pattern. */
    get(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP POST handler matching specified URL pattern. */
    post(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP OPTIONS handler matching specified URL pattern. */
    options(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP DELETE handler matching specified URL pattern. */
    del(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP PATCH handler matching specified URL pattern. */
    patch(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP PUT handler matching specified URL pattern. */
    put(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP HEAD handler matching specified URL pattern. */
    head(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP CONNECT handler matching specified URL pattern. */
    connect(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP TRACE handler matching specified URL pattern. */
    trace(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP handler matching specified URL pattern on any HTTP method. */
    any(pattern: RecognizedString, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers a handler matching specified URL pattern where WebSocket upgrade requests are caught. */
    ws(pattern: RecognizedString, behavior: WebSocketBehavior) : TemplatedApp;
}

/** Constructs a non-SSL app */
export function App(options: AppOptions): TemplatedApp;

/** Constructs an SSL app */
export function SSLApp(options: AppOptions): TemplatedApp;

/** Closes a uSockets listen socket. */
export function us_listen_socket_close(listenSocket: us_listen_socket): void;

/** WebSocket compression options */
export enum CompressOptions {
    /** No compression (always a good idea) */
    DISABLED,
    /** Zero memory overhead compression (recommended) */
    SHARED_COMPRESSOR,
    /** Sliding dedicated compress window, requires lots of memory per socket */
    DEDICATED_COMPRESSOR
}
