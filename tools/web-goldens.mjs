// @ts-check
// WebGPU screenshots vs the CPU goldens. Serves the built web bundle,
// screenshots each page in headless Chrome, and hands the pixel work to
// the glim-golden CLI.
//
//   node tools/web-goldens.mjs
//
// Env: GLIM_BUILD_DIR, GLIM_GOT_DIR, GLIM_GOLDEN_BIN, GLIM_WEB_BUDGET_MS,
// WEBGPU_ANGLE, CHROME_BIN.
import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import serve from "./executors/native/serve.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const build = process.env.GLIM_BUILD_DIR ?? path.join(root, "build/web-webgpu-debug");
const serveDir = path.join(build, "examples/web/serve");
const gotDir = process.env.GLIM_GOT_DIR ?? path.join(build, "web-got");
const golden = process.env.GLIM_GOLDEN_BIN ?? path.join(root, "build/glim-golden/glim-golden");
const budgetMs = process.env.GLIM_WEB_BUDGET_MS ?? "60000";
const angle = process.env.WEBGPU_ANGLE ?? "swiftshader";
const canvas = "0,33,720,480"; // page chrome above the canvas is cropped away
const attempts = 3;
const shotTimeoutMs = Number(budgetMs) + 30000;
// Fresh profile: a shared one can hand off to a running Chrome and hang.
const profile = mkdtempSync(path.join(os.tmpdir(), "glim-chrome-"));

/** @type {{name: string, page: string, golden: string, maxOver: number, maxDelta: number}[]} */
const cases = [
  { name: "web-hello", page: "index.html", golden: "examples/hello/golden/hello.png", maxOver: 2000, maxDelta: 48 },
  { name: "web-gradients", page: "gradients.html", golden: "examples/gradients/golden/gradients.png", maxOver: 4000, maxDelta: 48 },
  { name: "web-glass", page: "glass.html", golden: "examples/glass/golden/glass.png", maxOver: 280000, maxDelta: 224 },
];
const foreignPage = "foreign.html";
const required = ["index.html", "glass.html", "gradients.html", foreignPage, "glim-app.js", "glim-app.wasm", "main.js", "lib/glim.js"];

function fail(/** @type {string} */ message) {
  console.error(message);
  process.exit(1);
}

function findChrome() {
  const names = ["chrome", "google-chrome", "google-chrome-stable", "chromium", "chromium-browser"];
  const mac = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
  if (process.env.CHROME_BIN) return process.env.CHROME_BIN;
  for (const dir of (process.env.PATH ?? "").split(path.delimiter)) {
    for (const name of names) {
      const candidate = path.join(dir, name);
      if (existsSync(candidate)) return candidate;
    }
  }
  return existsSync(mac) ? mac : undefined;
}

/** Runs glim-golden; returns its exit code (stdout is printed). */
function glimGolden(/** @type {string[]} */ args) {
  const r = spawnSync(golden, args, { encoding: "utf8" });
  if (r.error) fail(`cannot run ${golden}: ${r.error.message}`);
  process.stdout.write(r.stdout);
  process.stderr.write(r.stderr);
  return r.status ?? 2;
}

const chrome = findChrome();
if (!chrome) fail("no Chrome found; set CHROME_BIN");
if (!existsSync(golden)) fail(`missing ${golden}; build with: cmake -S tools/glim-golden -B build/glim-golden`);
for (const f of required) {
  if (!existsSync(path.join(serveDir, f))) {
    fail(`missing serve file ${path.join(serveDir, f)}; build with: cmake --preset web-webgpu-debug`);
  }
}
mkdirSync(gotDir, { recursive: true });

const { server, port } = await serve.serveStatic(serveDir, 0);

// Async on purpose: the page is fetched from this process's own server,
// which a blocking spawn would starve.
function shot(/** @type {string} */ name, /** @type {string} */ page) {
  return new Promise((resolve) => {
    const child = spawn(chrome, [
      "--headless=new", `--user-data-dir=${profile}`, "--no-sandbox",
      "--disable-dev-shm-usage", "--enable-unsafe-swiftshader", `--use-angle=${angle}`,
      "--hide-scrollbars", "--force-device-scale-factor=1",
      "--window-size=720,513", `--virtual-time-budget=${budgetMs}`,
      `--screenshot=${path.join(gotDir, `${name}.png`)}`,
      `http://127.0.0.1:${port}/${page}?static=1`,
    ], { stdio: "ignore" });
    const timer = setTimeout(() => {
      console.error(`${name}: chrome timed out`);
      child.kill("SIGKILL");
    }, shotTimeoutMs);
    child.on("close", () => {
      clearTimeout(timer);
      resolve(undefined);
    });
  });
}

function drawn(/** @type {string} */ name) {
  return glimGolden(["blank", path.join(gotDir, `${name}.png`), "--crop", canvas]) === 0;
}

for (const c of cases) {
  let ready = false;
  for (let i = 0; i < attempts && !ready; i++) {
    await shot(c.name, c.page);
    ready = drawn(c.name);
  }
  if (!ready) console.error(`${c.name} still blank after ${attempts} shots`);
}

const foreign = await fetch(`http://127.0.0.1:${port}/${foreignPage}?static=1`);
if (!foreign.ok) fail(`foreign smoke failed: ${foreign.status}`);
console.log("foreign smoke ok");
server.close();
rmSync(profile, { recursive: true, force: true });

const blanks = cases.filter((c) => !drawn(c.name)).length;
if (blanks === cases.length && angle === "swiftshader") {
  console.log("SKIP: SwiftShader cannot present WebGPU canvas; pixel-compare needs real GPU (local Metal is green)");
  process.exit(0);
}

let failed = false;
for (const c of cases) {
  const code = glimGolden([
    "compare", path.join(gotDir, `${c.name}.png`), path.join(root, c.golden),
    "--crop", canvas, "--save", path.join(gotDir, `${c.name}-canvas.png`),
    "--max-over", String(c.maxOver), "--max-delta", String(c.maxDelta),
  ]);
  failed ||= code !== 0;
}
process.exit(failed ? 1 : 0);
