import { us_listen_socket } from "../../docs/index";
import { HttpRequest } from "./http-request";
import { HttpResponse } from "./http-response";
import { HttpRouter } from "./http-router";

export type ListenCallback = (listenSocket?: us_listen_socket | false) => void;

export type RouteHandler = (router: HttpRouter) => boolean;

export type HttpHandler = (res: HttpResponse, req: HttpRequest) => void;

export type Method =
  | "get"
  | "post"
  | "head"
  | "put"
  | "delete"
  | "connect"
  | "options"
  | "trace"
  | "patch";
