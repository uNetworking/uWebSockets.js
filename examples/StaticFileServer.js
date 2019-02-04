//! In-Memory Static File Server, Optimized For Low Latency, Optimal For Single Page Applications.

const fs = require("fs");
const path = require("path");
const uWS = require("uWebSockets.js");

const app = uWS.App();
const staticFiles = {};
const routes = {};
const serverPort = 8000;
const staticFilesDir = "public";
const mimeTypes = {
  html: "text/html",
  jpeg: "image/jpeg",
  jpg: "image/jpeg",
  png: "image/png",
  js: "text/javascript",
  css: "text/css",
  json: "application/json",
  wasm: "application/wasm"
};

Array.prototype.last = function() {
  return this[this.length - 1];
};

const walkDirSync = (currentDirPath, callback) => {
  fs.readdirSync(currentDirPath).forEach(dirFile => {
    const filePath = path.join(currentDirPath, dirFile);
    const fileStats = fs.statSync(filePath);

    if (fileStats.isFile()) {
      const fileData = fs.readFileSync(filePath, "utf8");
      callback(filePath, fileData, fileStats);
    } else if (fileStats.isDirectory()) {
      walkDirSync(filePath, callback);
    }
  });
};

walkDirSync(staticFilesDir, (filePath, fileData, fileStats) => {
  console.log("Loading : ", filePath);

  let route = filePath.replace(staticFilesDir, "");
  let fileExtension = route.split(".").last();

  staticFiles[route] = {
    mime: mimeTypes[fileExtension] || "",
    data: fileData || " " // uWS does not yet support empty response.
  };

  routes[route] = route;

  // Handle Routes for Known Files
  app.get(route, (res, req) =>
    res.end(handler(req, res, (route = req.getUrl())))
  );
});

const defaultStaticFileRoute = "/index.html";
const routeParser = route => {
  // If route has a `.`, return it.
  if (/\./.test(route)) return route;

  if (route !== "/") {
    // If route ends with `/` return
    // `index.html` else `/index.html`
    return route.endsWith("/")
      ? route + "index.html"
      : route + defaultStaticFileRoute;
  }
  return defaultStaticFileRoute;
};

const handler = (req, res, route = "") => {
  if (!route) {
    route = routes[req.getUrl()];
    if (!route) {
      const path = req.getUrl();
      // Here we add current `request url` to our routes object,
      // so we don't have to parse the same `request url` on every request.
      routes[path] = routeParser(path);
      route = routes[path];
    }
  }

  const routeValue = staticFiles[route];
  if (routeValue) {
    res.writeHeader("content-type", routeValue.mime);
    return routeValue.data;
  } else {
    res.writeStatus("404");
  }
};

app.get("/*", (res, req) => res.end(handler(req, res)));

app.listen(serverPort, token => {
  if (token) {
    console.log("Listening to port " + serverPort);
  } else {
    console.log("Failed to listen to port " + serverPort);
  }
});
