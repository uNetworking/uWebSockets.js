/* Under construction, lots of things to add */

/** Native type representing a raw uSockets struct us_listen_socket. */
interface us_listen_socket {

}

/** An HttpResponse is valid until either onAborted callback or any of the .end/.tryEnd calls succeed. You may attach user data to this object. */
interface HttpResponse {
    writeHeader(key: string, value: string) : void;
    /** Ends this response by copying the contents of body. */
    end(body: string) : void;
    /** Ends this response, or tries to, by streaming appropriately sized chunks of body. Use in conjunction with onWritable. Returns tuple [ok, hasResponded].*/
    tryEnd(fullBodyOrChunk: string, totalSize: number) : [boolean, boolean];
}

/** An HttpRequest is stack allocated and only accessible during the callback invocation. */
interface HttpRequest {
    getHeader(lowerCaseKey: string) : string;
}

/** TemplatedApp is either an SSL or non-SSL app. */
interface TemplatedApp {
    /** Listens to hostname & port. Callback hands either false or a listen socket. */
    listen(host: string, port: number, cb: (listenSocket: us_listen_socket) => void): TemplatedApp;
    /** Listens to port. Callback hands either false or a listen socket. */
    listen(port: number, cb: (listenSocket: any) => void): TemplatedApp;
    /** Registers an HTTP GET handler matching specified URL pattern. */
    get(pattern: string, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
    /** Registers an HTTP POST handler matching specified URL pattern. */
    post(pattern: string, handler: (res: HttpResponse, req: HttpRequest) => void) : TemplatedApp;
}

/** Options used when constructing an app. */
interface AppOptions {
    key_file_name?: string
    cert_file_name?: string
    passphrase?: string
}

/** Constructs a non-SSL app */
export function App(options: AppOptions): TemplatedApp;

/** Constructs an SSL app */
export function SSLApp(options: AppOptions): TemplatedApp;
