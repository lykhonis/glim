// @ts-check
// Static file server for built web bundles (nx run, web goldens).
const fs = require("fs");
const http = require("http");
const path = require("path");

/** @type {Record<string, string>} */
const mime = {
  ".html": "text/html",
  ".js": "text/javascript",
  ".mjs": "text/javascript",
  ".wasm": "application/wasm",
  ".png": "image/png",
  ".json": "application/json",
  ".css": "text/css",
};

/**
 * Serves `dir` on 127.0.0.1:`port` (0 picks a free port).
 * @param {string} dir
 * @param {number} port
 * @returns {Promise<{ server: http.Server, port: number }>}
 */
function serveStatic(dir, port) {
  const base = path.resolve(dir);
  const server = http.createServer((req, res) => {
    const rel = decodeURIComponent(new URL(req.url ?? "/", "http://x").pathname);
    const file = path.join(base, rel === "/" ? "index.html" : rel);
    if (!file.startsWith(base) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
      res.writeHead(404).end();
      return;
    }
    res.writeHead(200, { "content-type": mime[path.extname(file)] ?? "application/octet-stream" });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(port, "127.0.0.1", () => {
      const address = server.address();
      resolve({ server, port: typeof address === "object" && address ? address.port : port });
    });
  });
}

module.exports = { serveStatic };
