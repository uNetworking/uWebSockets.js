/** Native type representing a raw uSockets struct us_listen_socket.
 * Careful with this one, it is entirely unchecked and native so invalid usage will blow up.
 */
export interface us_listen_socket {

}

/** Recognized string types, things C++ can read and understand as strings.
 * "String" does not have to mean "text", it can also be "binary".
 */
export type RecognizedString = string | ArrayBuffer | Uint8Array | Int8Array | Uint16Array | Int16Array | Uint32Array | Int32Array | Float32Array | Float64Array;

/** A WebSocket connection that is valid from open to close event.
 * Read more about this in the user manual.
 */
export type WebSocket<UserData = {
    /** Arbitrary user data may be attached to this object. In C++ this is done by using getUserData(). */
    [key: string]: any;
}> = {
    /** Sends a message. Make sure to check getBufferedAmount() before sending. Returns true for success, false for built up backpressure that will drain when time is given.
     * Returning false does not mean nothing was sent, it only means backpressure was built up. This you can check by calling getBufferedAmount() afterwards.
     *
     * Make sure you properly understand the contept of backpressure. Check the backpressure example file.
     */
    send(message: RecognizedString, isBinary?: boolean, compress?: boolean) : boolean;

    /** Returns the bytes buffered in backpressure. This is similar to the bufferedAmount property in the browser counterpart.
     * Check backpressure example.
     */
    getBufferedAmount() : number;

    /** Gracefully closes this WebSocket. Immediately calls the close handler.
     * A WebSocket close message is sent with code and shortMessage.
     */
    end(code?: number, shortMessage?: RecognizedString) : WebSocket;

    /** Forcefully closes this WebSocket. Immediately calls the close handler.
     * No WebSocket close message is sent.
     */
    close() : WebSocket;

    /** Sends a ping control message. Returns true on success in similar ways as WebSocket.send does (regarding backpressure). This helper function correlates to WebSocket::send(message, uWS::OpCode::PING, ...) in C++. */
    ping(message?: RecognizedString) : boolean;

    /** Subscribe to a topic in MQTT syntax. Subscription is immediately active.
     * MQTT syntax includes things like "root/child/+/grandchild" where "+" is a
     * wildcard and "root/#" where "#" is a terminating wildcard.
    */
    subscribe(topic: RecognizedString) : WebSocket;

    /** Unsubscribe from a topic. Returns true on success, if the WebSocket was subscribed. Unsubscription is immediately active. */
    unsubscribe(topic: RecognizedString) : boolean;

    /** Unsubscribe from all topics. Immediately active. This is called automatically before any close handler is called, so you never need to call this manually in the close handler of a WebSocket. */
    unsubscribeAll() : void;

    /** Publish a message to a topic in MQTT syntax. You cannot publish using wildcards, only fully specified topics. Just like with MQTT.
     *
     * "parent/child" kind of tree is allowed, but not "parent/#" kind of wildcard publishing.
     *
     * NOTE: publish is NOT immediately active, the actual publish takes place at the end of the current event loop iteration.
     * This to efficiently pack and batch outgoing publishes, and to coalesce outgoing messages across multiple topics.
     * Publishing to many different topics, or to the same topic many times, will thus be properly coalesced into one single send syscall and one single SSL block, per socket.
     *
     * This also means that publishing to a topic, then immediately subscribing to it in the same very event loop iteration will cause that publish to also affect the late subscription.
     * Think of it as - subscription vs. publish happens in terms of event loop iterations. If you REQUIRE a strict ordering you have to use Loop::defer or process.nextTick accordingly.
     */
    publish(topic: RecognizedString, message: RecognizedString, isBinary?: boolean, compress?: boolean) : WebSocket;

    /** See HttpResponse.cork. Takes a function in which the socket is corked (packing many sends into one single syscall/SSL block) */
    cork(cb: () => void) : void;

    /** Returns the remote IP address. Note that the returned IP is binary, not text.
     *
     * IPv4 is 4 byte long and can be converted to text by printing every byte as a digit between 0 and 255.
     * IPv6 is 16 byte long and can be converted to text in similar ways, but you typically print digits in HEX.
     *
     * We will probably add a text converting function at some point as this is a common issue among users.
     */
    getRemoteAddress() : ArrayBuffer;
} & UserData

/** An HttpResponse is valid until either onAborted callback or any of the .end/.tryEnd calls succeed. You may attach user data to this object. */
export interface HttpResponse {
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
    onAborted(handler: () => void) : HttpResponse;

    /** Handler for reading data from POST and such requests. You MUST copy the data of chunk if isLast is not true. We Neuter ArrayBuffers on return, making it zero length.*/
    onData(handler: (chunk: ArrayBuffer, isLast: boolean) => void) : HttpResponse;

    /** Returns the remote IP address */
    getRemoteAddress() : ArrayBuffer;

    /** Corking a response is a performance improvement in both CPU and network, as you ready the IO system for writing multiple chunks at once.
     * By default, you're corked in the immediately executing top portion of the route handler. In all other cases, such as when returning from
     * await, or when being called back from an async database request or anything that isn't directly executing in the route handler, you'll want
     * to cork before calling writeStatus, writeHeader or just write. Corking takes a callback in which you execute the writeHeader, writeStatus and
     * such calls, in one atomic IO operation. This is important, not only for TCP but definitely for TLS where each write would otherwise result
     * in one TLS block being sent off, each with one send syscall.
     *
     * Example usage:
     *
     * res.cork(() => {
     *   res.writeStatus("200 OK").writeHeader("Some", "Value").write("Hello world!");
     * });
     */
    cork(cb: () => void) : void;

    /** Arbitrary user data may be attached to this object */
    [key: string]: any;
}

/** An HttpRequest is stack allocated and only accessible during the callback invocation. */
export interface HttpRequest {
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
    /** Setting yield to true is to say that this route handler did not handle the route, causing the router to continue looking for a matching route handler, or fail. */
    setYield(yield: boolean) : HttpRequest;
}

/** A structure holding settings and handlers for a WebSocket URL route handler. */
export interface WebSocketBehavior {
    /** Maximum length of received message. If a client tries to send you a message larger than this, the connection is immediately closed. */
    maxPayloadLength?: number;
    /** Maximum amount of seconds that may pass without sending or getting a message. Connection is closed if this timeout passes. Resolution (granularity) for timeouts are typically 4 seconds, rounded to closest.
     * Disable by leaving 0.
     */
    idleTimeout?: number;
    /** What permessage-deflate compression to use. uWS.DISABLED, uWS.SHARED_COMPRESSOR or any of the uWS.DEDICATED_COMPRESSOR_xxxKB. */
    compression?: CompressOptions;
    /** Maximum length of allowed backpressure per socket when PUBLISHING messages (does not apply to ws.send). Slow receivers with too high backpressure will be skipped until they catch up or timeout. */
    maxBackpressure?: number;
    /** Handler for new WebSocket connection. WebSocket is valid from open to close, no errors.
     * You may access the HttpRequest during the lifetime of the callback (until first await or return).
     */
    open?: (ws: WebSocket, req: HttpRequest) => void;
    /** Handler for a WebSocket message. Messages are given as ArrayBuffer no matter if they are binary or not. Given ArrayBuffer is valid during the lifetime of this callback (until first await or return) and will be neutered. */
    message?: (ws: WebSocket, message: ArrayBuffer, isBinary: boolean) => void;
    /** Handler for when WebSocket backpressure drains. Check ws.getBufferedAmount(). Use this to guide / drive your backpressure throttling. */
    drain?: (ws: WebSocket) => void;
    /** Handler for close event, no matter if error, timeout or graceful close. You may not use WebSocket after this event. Do not send on this WebSocket from within here, it is closed. */
    close?: (ws: WebSocket, code: number, message: ArrayBuffer) => void;
    /** Handler for received ping control message. You do not need to handle this, pong messages are automatically sent as per the standard. */
    ping?: (ws: WebSocket) => void;
    /** Handler for received pong control message. */
    pong?: (ws: WebSocket) => void;
}

/** Options used when constructing an app. Especially for SSLApp.
 * These are options passed directly to uSockets, C layer.
 */
export interface AppOptions {
    key_file_name?: RecognizedString;
    cert_file_name?: RecognizedString;
    passphrase?: RecognizedString;
    dh_params_file_name?: RecognizedString;
    /** This translates to SSL_MODE_RELEASE_BUFFERS */
    ssl_prefer_low_memory_usage?: boolean;
}

/** TemplatedApp is either an SSL or non-SSL app. See App for more info, read user manual. */
export interface TemplatedApp {
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
    /** Publishes a message under topic, for all WebSockets under this app. See WebSocket.publish. */
    publish(topic: RecognizedString, message: RecognizedString, isBinary?: boolean, compress?: boolean) : TemplatedApp;
}

/** Constructs a non-SSL app. An app is your starting point where you attach behavior to URL routes.
 * This is also where you listen and run your app, set any SSL options (in case of SSLApp) and the like.
 */
export function App(options?: AppOptions): TemplatedApp;

/** Constructs an SSL app. See App. */
export function SSLApp(options: AppOptions): TemplatedApp;

/** Closes a uSockets listen socket. */
export function us_listen_socket_close(listenSocket: us_listen_socket): void;

/** WebSocket compression options */
export type CompressOptions = number;
/** No compression (always a good idea if you operate using an efficient binary protocol) */
export var DISABLED: CompressOptions;
/** Zero memory overhead compression (recommended for pub/sub where same message is sent to many receivers) */
export var SHARED_COMPRESSOR: CompressOptions;
/** Sliding dedicated compress window, requires 3KB of memory per socket */
export var DEDICATED_COMPRESSOR_3KB: CompressOptions;
/** Sliding dedicated compress window, requires 4KB of memory per socket */
export var DEDICATED_COMPRESSOR_4KB: CompressOptions;
/** Sliding dedicated compress window, requires 8KB of memory per socket */
export var DEDICATED_COMPRESSOR_8KB: CompressOptions;
/** Sliding dedicated compress window, requires 16KB of memory per socket */
export var DEDICATED_COMPRESSOR_16KB: CompressOptions;
/** Sliding dedicated compress window, requires 32KB of memory per socket */
export var DEDICATED_COMPRESSOR_32KB: CompressOptions;
/** Sliding dedicated compress window, requires 64KB of memory per socket */
export var DEDICATED_COMPRESSOR_64KB: CompressOptions;
/** Sliding dedicated compress window, requires 128KB of memory per socket */
export var DEDICATED_COMPRESSOR_128KB: CompressOptions;
/** Sliding dedicated compress window, requires 256KB of memory per socket */
export var DEDICATED_COMPRESSOR_256KB: CompressOptions;
