// @ts-check
// Typing into a Glim text field in Chrome. Serves the web build of the
// text example, drives the page over the DevTools protocol (click the
// field, insert text, Backspace) and reads the values the example logs.
//
//   node tools/web-text-e2e.mjs
//
// Env: GLIM_BUILD_DIR, WEBGPU_ANGLE, CHROME_BIN.
import { spawn } from "node:child_process";
import { copyFileSync, existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import serve from "./executors/native/serve.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const build = process.env.GLIM_BUILD_DIR ?? path.join(root, "build/web-webgpu-debug");
const example = path.join(build, "examples/text");
const angle = process.env.WEBGPU_ANGLE ?? "swiftshader";
const timeoutMs = 60000;

/** @param {string} message @returns {never} */
function fail(message) {
  console.error(`FAIL: ${message}`);
  process.exit(1);
}

function findChrome() {
  if (process.env.CHROME_BIN) return process.env.CHROME_BIN;
  const names = ["chrome", "google-chrome", "google-chrome-stable", "chromium", "chromium-browser"];
  for (const dir of (process.env.PATH ?? "").split(path.delimiter)) {
    for (const name of names) {
      if (existsSync(path.join(dir, name))) return path.join(dir, name);
    }
  }
  const mac = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
  return existsSync(mac) ? mac : undefined;
}

const site = mkdtempSync(path.join(os.tmpdir(), "glim-web-text-"));
for (const f of ["glim-text.js", "glim-text.wasm"]) {
  const from = path.join(example, f);
  if (!existsSync(from)) fail(`missing ${from}; build glim-text with the web preset`);
  copyFileSync(from, path.join(site, f));
}
writeFileSync(
  path.join(site, "index.html"),
  `<!doctype html><html><body style="margin:0">
<canvas id="glim" width="480" height="320" style="width:480px;height:320px"></canvas>
<script>var Module = { canvas: document.getElementById("glim") };</script>
<script src="glim-text.js"></script></body></html>`,
);

const chromeBin = findChrome() ?? fail("no Chrome found; set CHROME_BIN");
const { server, port } = await serve.serveStatic(site, 0);
const profile = mkdtempSync(path.join(os.tmpdir(), "glim-chrome-"));
const chrome = spawn(chromeBin, [
  "--headless=new", `--user-data-dir=${profile}`, "--no-sandbox", "--disable-dev-shm-usage",
  "--enable-unsafe-webgpu", "--enable-unsafe-swiftshader", `--use-angle=${angle}`,
  "--remote-debugging-port=0", "--window-size=480,320", "about:blank",
]);
const timer = setTimeout(() => {
  chrome.kill("SIGKILL");
  fail("timed out");
}, timeoutMs);

const devtools = await new Promise((resolve) => {
  let err = "";
  chrome.stderr.on("data", (d) => {
    err += d;
    const m = err.match(/DevTools listening on (ws:\/\/\S+)/);
    if (m) resolve(m[1]);
  });
});
const { webSocketDebuggerUrl } = (
  await (await fetch(`http://${new URL(devtools).host}/json/list`)).json()
).find((/** @type {{type: string}} */ t) => t.type === "page");

const ws = new WebSocket(webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener("open", r));
let nextId = 0;
/** @type {Map<number, (v: any) => void>} */
const pending = new Map();
/** @type {string[]} */
const logs = [];
ws.addEventListener("message", (ev) => {
  const msg = JSON.parse(String(ev.data));
  if (msg.id !== undefined) pending.get(msg.id)?.(msg);
  if (msg.method === "Runtime.consoleAPICalled") {
    logs.push(msg.params.args.map((/** @type {{value?: unknown}} */ a) => String(a.value)).join(" "));
  }
});
/** @param {string} method @param {object} [params] */
const send = (method, params = {}) =>
  new Promise((resolve) => {
    const id = ++nextId;
    pending.set(id, resolve);
    ws.send(JSON.stringify({ id, method, params }));
  });
/** @param {string} expression */
const evaluate = async (expression) =>
  /** @type {any} */ (await send("Runtime.evaluate", { expression, returnByValue: true })).result.result.value;
/** @param {() => Promise<boolean>} done @param {string} what */
const until = async (done, what) => {
  for (let i = 0; i < 100; i++) {
    if (await done()) return;
    await new Promise((r) => setTimeout(r, 200));
  }
  fail(`never ${what}; console: ${logs.join(" | ")}`);
};
const click = async (/** @type {number} */ x, /** @type {number} */ y) => {
  for (const type of ["mousePressed", "mouseReleased"]) {
    await send("Input.dispatchMouseEvent", { type, x, y, button: "left", clickCount: 1 });
  }
};

await send("Runtime.enable");
await send("Page.navigate", { url: `http://127.0.0.1:${port}/index.html` });
// The field sits at the top left; tap until the app is up and the text
// bridge holds focus.
await until(async () => {
  await click(40, 16);
  return (await evaluate("document.activeElement === Module.glimTextInput")) === true;
}, "focused the field");
await send("Input.insertText", { text: "glim" });
await until(async () => logs.includes("text value glim"), "typed glim");
await send("Input.dispatchKeyEvent", { type: "rawKeyDown", key: "Backspace", code: "Backspace", windowsVirtualKeyCode: 8 });
await send("Input.dispatchKeyEvent", { type: "keyUp", key: "Backspace", code: "Backspace", windowsVirtualKeyCode: 8 });
await until(async () => logs.includes("text value gli"), "deleted backward");

// Glim's edit menu: select all, right-click the field, pick Cut (the
// first item); the field empties and the system clipboard holds the text.
await send("Browser.grantPermissions", {
  origin: `http://127.0.0.1:${port}`,
  permissions: ["clipboardReadWrite", "clipboardSanitizedWrite"],
});
await send("Input.dispatchKeyEvent", { type: "rawKeyDown", key: "a", code: "KeyA", windowsVirtualKeyCode: 65, modifiers: 4, commands: ["selectAll"] });
await send("Input.dispatchKeyEvent", { type: "keyUp", key: "a", code: "KeyA", windowsVirtualKeyCode: 65, modifiers: 4 });
await new Promise((r) => setTimeout(r, 500));
for (const type of ["mousePressed", "mouseReleased"]) {
  await send("Input.dispatchMouseEvent", { type, x: 60, y: 16, button: "right", clickCount: 1 });
}
await new Promise((r) => setTimeout(r, 500));
await click(78, 30);
await until(async () => logs.includes("text value "), "cut the field from the menu");
await until(
  async () => (await send("Runtime.evaluate", { expression: "navigator.clipboard.readText()", awaitPromise: true, returnByValue: true }).then((/** @type {any} */ m) => m.result.result.value)) === "gli",
  "put the cut text on the clipboard",
);

console.log("web text: focused, typed glim, deleted to gli, cut gli from the edit menu");
clearTimeout(timer);
ws.close();
const exited = new Promise((r) => chrome.on("close", r));
chrome.kill();
await exited;
server.close();
rmSync(profile, { recursive: true, force: true });
rmSync(site, { recursive: true, force: true });
