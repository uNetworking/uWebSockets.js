import { HttpResponse } from "./http-response";
import { HttpRequest } from "./http-request";
import { Method, RouteHandler } from "./types";
import { IncomingMessage, ServerResponse } from "http";

type USERDATA = {
  httpRequest: HttpRequest;
  httpResponse: HttpResponse;
};

const MAX_URL_SEGMENTS = 100 as const;

/**
 * int64 (ish) Bitwise OR
 * @param v1
 * @param v2
 */
function OR(v1: number, v2: number) {
  var hi = 0x80000000;
  var low = 0x7fffffff;
  var hi1 = ~~(v1 / hi);
  var hi2 = ~~(v2 / hi);
  var low1 = v1 & low;
  var low2 = v2 & low;
  var h = hi1 | hi2;
  var l = low1 | low2;
  return h * hi + l;
}

/**
 * int64 (ish) Bitwise AND
 * @param v1
 * @param v2
 */
function AND(v1: number, v2: number) {
  var hi = 0x80000000;
  var low = 0x7fffffff;
  var hi1 = ~~(v1 / hi);
  var hi2 = ~~(v2 / hi);
  var low1 = v1 & low;
  var low2 = v2 & low;
  var h = hi1 & hi2;
  var l = low1 & low2;
  return h * hi + l;
}

/**
 * Given an array
 *
 * The elements are compared `comp`. The elements in the range shall already be
 * sorted according to this same criterion (`comp`), or at least partitioned
 * with respect to val.
 *
 * The function optimizes the number of comparisons performed by comparing
 * non-consecutive elements of the sorted range.
 *
 * The index into the `array` returned by this function cannot be equivalent to
 * `val`, only greater.
 *
 * On average, logarithmic in the distance of the length of the array: Performs
 * approximately `log2(N)+1` element comparisons (where `N` is this length).
 *
 * @param array
 * @param val Value of the upper bound to search for in the range.
 * @param comp A function that accepts two arguments (the first is always
 * `val`, and the second from the given `array`, and returns bool. The value
 * returned indicates whether the first argument is considered to go before the
 * second.
 *
 * @returns The index to the upper bound position for `val` in the range. If no
 * element in the range compares greater than `val`, the function returns
 * `array.length`.
 */
function upperBound<T>(array: T[], val: T, comp: (a: T, b: T) => boolean) {
  let count = array.length;

  let first = 0;
  let offset = 0;
  while (count > 0) {
    let step = (count / 2) | 0;
    offset += step;
    if (!comp(val, array[offset])) {
      first = ++offset;
      count -= step + 1;
    } else {
      count = step;
      offset = first;
    }
  }
  return first;
}

class Node {
  children: Node[];
  name: string;
  isHighPriority: boolean;
  handlers: number[];

  constructor(name: string) {
    this.name = name;
    this.children = [];
    this.handlers = [];
    this.isHighPriority = false;
  }
}

/**
 * Basically a pre-allocated stack
 */
class RouteParameters {
  params: string[] = Array(MAX_URL_SEGMENTS).fill("");
  paramsTop: number = 0;
  public reset() {
    this.paramsTop = -1;
  }
  public push(param: string) {
    this.params[++this.paramsTop] = param;
  }
  public pop() {
    this.paramsTop--;
  }
}

enum Priority {
  HIGH_PRIORITY = 0xd0000000,
  MEDIUM_PRIORITY = 0xe0000000,
  LOW_PRIORITY = 0xf0000000
}

/**
 * Handler ids are 32-bit
 */
const HANDLER_MASK = 0x0fffffff as const;

export class HttpRouter {
  public methods = [
    "get",
    "post",
    "head",
    "put",
    "delete",
    "connect",
    "options",
    "trace",
    "patch"
  ] as const;

  public HIGH_PRIORITY = Priority.HIGH_PRIORITY as const;
  public MEDIUM_PRIORITY = Priority.MEDIUM_PRIORITY as const;
  public LOW_PRIORITY = Priority.LOW_PRIORITY as const;

  /**
   * Methods and their respective priority
   */
  public priority: Map<string, number>;

  /**
   * List of handlers
   */
  public handlers: RouteHandler[];

  /**Current URL cache */
  public currentUrl: string | null;
  public urlSegmentVector: string[];
  public urlSegmentTop: number;

  /**
   * The matching tree
   */
  public root: Node = new Node("rootNode");

  public routeParameters: RouteParameters = new RouteParameters();

  public userData: USERDATA | null;

  constructor() {
    this.priority = new Map<string, number>();
    this.handlers = [];
    this.currentUrl = null;
    this.urlSegmentVector = [];
    this.urlSegmentTop = 0;
    this.userData = null;

    let p = 0;
    for (const method of this.methods) {
      this.priority.set(method, p++);
    }
  }

  getParameters() {
    return [this.routeParameters.paramsTop, this.routeParameters.params] as [
      number,
      string[]
    ];
  }

  setUserData(req: IncomingMessage, res: ServerResponse) {
    this.userData = {
      httpRequest: new HttpRequest(req),
      httpResponse: new HttpResponse(res, req)
    };
  }

  getUserData() {
    return this.userData;
  }

  /**
   * Fast path
   */
  route(method: string, url: string) {
    if (!this.userData) {
      throw new Error("User Data not set in Http Router; this should not happen.");
    }

    // Reset url parsing cache
    this.setUrl(url);
    this.routeParameters.reset();

    // Begin by finding the method node
    for (let p of this.root.children) {
      if (p.name === method) {
        /* Then route the url */
        return this.executeHandlers(p, 0, this.userData);
      }
    }

    /* We did not find any handler for this method and url */
    return false;
  }

  add(
    methods: readonly Method[],
    pattern: string,
    handler: RouteHandler,
    priority: Priority = Priority.MEDIUM_PRIORITY
  ) {
    for (let method of methods) {
      // Lookup method
      let node = this.getNode(this.root, method, false);
      // Iterate over all segments
      this.setUrl(pattern);
      for (let i = 0; !this.getUrlSegment(i)[1]; i++) {
        node = this.getNode(
          node,
          this.getUrlSegment(i)[0],
          priority === Priority.HIGH_PRIORITY
        );
      }
      // Insert handler in order sorted by priority (most significant 1 byte)
      const size = this.handlers.length;
      const bit = OR(priority, size);
      const position = upperBound(node.handlers, bit, (a, b) => {
        return a < b;
      });
      node.handlers.splice(position, 0, bit);
    }

    // Store this handler
    this.handlers.push(handler);
  }

  /**
   * Executes as many handlers it can
   */
  executeHandlers(
    this: HttpRouter,
    parent: Node,
    urlSegment: number,
    _userData: USERDATA
  ) {
    this.userData = _userData;
    const [segment, isStop] = this.getUrlSegment(urlSegment);

    // If we are on STOP, return where we may stand
    if (isStop) {
      // We have reached accross the entire URL with no stoppage, execute
      for (let handler of parent.handlers) {
        if (this.handlers[AND(handler, HANDLER_MASK)](this)) {
          return true;
        }
      }
      // We reached the end, so go back
      return false;
    }

    for (let p of parent.children) {
      if (p.name.length && p.name[0] === "*") {
        // Wildcard match (can be seen as a shortcut)
        for (let handler of p.handlers) {
          if (this.handlers[handler & HANDLER_MASK](this)) {
            return true;
          }
        }
      } else if (p.name.length && p.name[0] === ":" && segment.length) {
        // Parameter match
        this.routeParameters.push(segment);
        if (this.executeHandlers(p, urlSegment + 1, this.userData)) {
          return true;
        }
        this.routeParameters.pop();
      } else if (p.name == segment) {
        // Static match
        if (this.executeHandlers(p, urlSegment + 1, this.userData)) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Set URL for router. Will reset any URL cache
   * @param url
   */
  setUrl(url: string) {
    /* We expect to stand on a slash */

    this.currentUrl = url;
    this.urlSegmentTop = -1;
  }

  /**
   * Advance from parent to child, adding child if necessary
   * @param parent
   * @param child
   * @param isHighPriority
   */
  getNode(parent: Node, child: string, isHighPriority: boolean) {
    for (let node of parent.children) {
      if (node.name === child && node.isHighPriority === isHighPriority) {
        return node;
      }
    }

    /* Insert sorted, but keep order if parent is root (we sort methods by priority elsewhere) */
    const newNode = new Node(child);
    newNode.isHighPriority = isHighPriority;
    const index = upperBound(parent.children, newNode, (a: Node, b: Node) => {
      if (a.isHighPriority !== b.isHighPriority) {
        return a.isHighPriority;
      }
      return b.name.length > 0 && parent !== this.root && b.name < a.name;
    });
    parent.children.splice(index, 0, newNode);

    return newNode;
  }

  /**
   * Lazily parse or read from cache
   * @param urlSegment
   */
  getUrlSegment(urlSegment: number) {
    if (urlSegment > this.urlSegmentTop && this.currentUrl !== null) {
      // Signal as STOP when we have no more URL or stack space
      if (!this.currentUrl.length || urlSegment > 99) {
        return ["", true] as [string, boolean];
      }

      // We always stand on a slash here, so step over it
      this.currentUrl = this.currentUrl.slice(1);

      let segmentLength = this.currentUrl.indexOf("/");
      if (segmentLength === -1) {
        segmentLength = this.currentUrl.length;

        // Push to url segment vector
        this.urlSegmentVector[urlSegment] = this.currentUrl.substr(0, segmentLength);
        this.urlSegmentTop++;

        // Update currentUrl
        this.currentUrl = this.currentUrl.substr(segmentLength);
      } else {
        // Push to url segment vector
        this.urlSegmentVector[urlSegment] = this.currentUrl.substr(0, segmentLength);
        this.urlSegmentTop++;

        // Update currentUrl
        this.currentUrl = this.currentUrl.substr(segmentLength);
      }
    }
    /* In any case we return it */
    return [this.urlSegmentVector[urlSegment], false] as [string, boolean];
  }
}
