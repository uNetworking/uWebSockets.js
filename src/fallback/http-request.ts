import { HttpRequest as uWsHttpRequest } from "../../docs/index";
import { IncomingMessage } from "http";
import { URL } from "url";
import { parse } from "querystring";

export class HttpRequest implements uWsHttpRequest {
  static MAX_HEADERS: 50;
  private ancientHttp: boolean;
  private didYield: boolean;
  private currentParameters: [number, string[]];
  private request: IncomingMessage;

  constructor(request: IncomingMessage) {
    this.request = request;
    this.ancientHttp = false;
    this.didYield = false;
    this.currentParameters = [0, []];
  }

  public isAncient() {
    return this.ancientHttp;
  }

  public getYield() {
    return this.didYield;
  }

  /**
   * If you do not want to handle this route
   * @param yield
   */
  public setYield(yielded: boolean) {
    this.didYield = yielded;
    return this;
  }

  public getHeader(lowerCasedHeader: string) {
    return this.request.headers[lowerCasedHeader] as string || "";
  }

  public getUrl() {
    if (!this.request.url) {
      throw new Error("URL on request is undefined");
    }

    return this.request.url;
  }

  public getMethod() {
    if (!this.request.method) {
      throw new Error("Method on request is undefined");
    }

    return this.request.method;
  }

  /**
   * Returns the raw querystring as a whole, still encoded
   */
  public getQuery(): string;
  /**
   * Finds and decodes the URI component.
   * @param key
   */
  public getQuery(key?: string): string {
    if (!this.request.url) {
      throw new Error("URL on request is undefined");
    }

    const search = new URL(this.request.url).search;
    if (key != null) {
      return parse(key)[key] as string || "";
    } else {
      return search;
    }
  }

  public setParameters(parameters: [number, string[]]) {
    this.currentParameters = parameters;
  }

  public getParameter(index: number) {
    if (this.currentParameters[0] < index) {
      return "";
    } else {
      return this.currentParameters[1][index];
    }
  }

  forEach(cb: (key: string, value: string) => void) {
    throw new Error("Not implemented");
  }
}
