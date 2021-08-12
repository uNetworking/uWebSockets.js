import http, { IncomingMessage, ServerResponse } from "http";
import { Socket, ListenOptions } from "net";
import { RecognizedString, WebSocketBehavior } from "../../docs/index";
import InternalWebSocket from "ws";

import { HttpRouter } from "./http-router";
import { HttpHandler, ListenCallback, Method } from "./types";
import { WebSocket } from "./websocket";

export class HttpContext {
  router: HttpRouter;
  http: http.Server | null;
  closed: boolean;
  wsServer: InternalWebSocket.Server | null;
  wsBehaviors: Map<RecognizedString, WebSocketBehavior>;

  constructor() {
    this.router = new HttpRouter();
    this.closed = false;
    this.http = null;
    this.wsServer = null;
    this.wsBehaviors = new Map<RecognizedString, WebSocketBehavior>();
  }

  public onHttp(
    method: Method | "*",
    pattern: RecognizedString,
    handler: HttpHandler,
    upgrade: boolean = false
  ) {
    // Todo: This is ugly, fix
    let methods: readonly Method[];
    if (method === "*") {
      methods = this.router.methods;
    } else {
      methods = [method];
    }

    this.router.add(
      methods,
      pattern.toString(),
      (r: HttpRouter) => {
        const user = r.getUserData();

        if (user === null) {
          throw new Error("Could not retrieve user data; this should not happen.")
        }

        user.httpRequest.setYield(false);
        user.httpRequest.setParameters(r.getParameters());

        /* Middleware? Automatically respond to expectations */
        const expect = user.httpRequest.getHeader("expect");
        if (expect != null && expect == "100-continue") {
          user.httpResponse.writeContinue();
        }

        handler(user.httpResponse, user.httpRequest);

        /* If any handler yielded, the router will keep looking for a suitable handler. */
        if (user.httpRequest.getYield()) {
          return false;
        }
        return true;
      },
      method === "*"
        ? this.router.LOW_PRIORITY
        : upgrade
        ? this.router.HIGH_PRIORITY
        : this.router.MEDIUM_PRIORITY
    );
  }

  public onWs(pattern: RecognizedString, behavior: WebSocketBehavior) {
    // We only need to create the WebSocket Server once, but we shouldn't
    // create it if the user never calls `TemplatedApp.ws(...)`
    if (!this.wsServer) {
      // `noServer: true` is necessary for us to be able to call `handleUpgrade`
      // called in `TemplatedApp.upgradeHandler`
      this.wsServer = new InternalWebSocket.Server({ noServer: true });
    }

    this.wsBehaviors.set(pattern, behavior);
  }

  handleRequest(req: IncomingMessage, res: ServerResponse) {
    this.router.setUserData(req, res);
    if (!req.method || !req.url || !this.router.route(req.method.toLowerCase(), req.url)) {
      res.destroy();
      return null;
    }
  }

  private handleHttpUpgrade(request: IncomingMessage, socket: Socket, head: Buffer) {
    const pathname = request.url;
    if (this.wsServer !== null) {
      const patterns = this.wsBehaviors.keys();

      for (const pattern of patterns) {
        if (pathname === pattern) {
          const behavior = this.wsBehaviors.get(pattern)!;

          if (typeof behavior.upgrade === "function") {
            // TODO: custom upgrade handler functionality isn't required
            // yet in Ganache, so this isn't implemented currently
            throw new Error("not implemented");
          } else {
            this.wsServer.handleUpgrade(request, socket, head, (internalWs) => {
              const ws = new WebSocket(internalWs);
              ws.initialize(behavior);
              this.wsServer!.emit("connection", internalWs, request);
            });
          }

          break;
        }
      }
    } else {
      socket.end("HTTP/1.1 400 Unexpected server response\n\n");
    }
  }

  private sockets = new Set<Socket>();
  listen(host: RecognizedString | undefined, port: number, callback: ListenCallback) {
    const server = this.http = http.createServer(this.handleRequest.bind(this));
    server.on("connection", socket => {
      server.once("close", () => {
        this.sockets.delete(socket);
      });
    });
    server.on("upgrade", this.handleHttpUpgrade.bind(this));

    const handleError = (err: Error & {code: string, syscall: string}) => {
      if (err.syscall === "listen") {
        server.removeListener("error", handleError);
        callback(false);
      }
      else throw err;
    }
    server.on("error", handleError);
    server.on("listening", () => server.removeListener("error", handleError));

    const options: ListenOptions = { port, exclusive: true };
    if (host) {
      options.host = host.toString();
    }

    server.listen({ port, exclusive: true }, () => {
      this.closed = false;
      callback(this);
    });
  }

  close(cb?: Function) {
    this.closed = true;
    for (const socket of this.sockets) {
      socket.destroy();
      this.sockets.delete(socket);
    }

    if (this.wsServer) {
      this.wsServer.close();
    }

    if (this.http) {
      this.http.close(() => {
        this.http = null;
        if (cb) {
          cb();
        }
      });
    } else {
      if (cb) {
        cb();
      }
    }
  }
}
