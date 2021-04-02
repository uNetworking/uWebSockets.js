const wrapMiddleware = require('./wrap-middleware');
const websocketUrl = require('./websocket-url');

module.exports = function addWsMethod(target) {
  /* This prevents conflict with other things setting `.ws`. */
  if (target.ws === null || target.ws === undefined) {
    target.ws = function addWsRoute(route, ...middlewares) {
      const wrappedMiddlewares = middlewares.map(wrapMiddleware);

      /* We append `/.websocket` to the route path here. Why? To prevent conflicts when
       * a non-WebSocket request is made to the same GET route - after all, we are only
       * interested in handling WebSocket requests.
       *
       * Whereas the original `express-ws` prefixed this path segment, we suffix it -
       * this makes it possible to let requests propagate through Routers like normal,
       * which allows us to specify WebSocket routes on Routers as well \o/! */
      const wsRoute = websocketUrl(route);

      /* Here we configure our new GET route. It will never get called by a client
       * directly, it's just to let our request propagate internally, so that we can
       * leave the regular middleware execution and error handling to Express. */
      this.get(...[wsRoute].concat(wrappedMiddlewares));

      /*
       * Return `this` to allow for chaining:
       */
      return this;
    };
  }
}
