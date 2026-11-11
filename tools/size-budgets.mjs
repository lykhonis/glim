// @ts-check
// Size budgets (docs decisions.md § Budgets, hardening H5) measured on
// release builds. Exits non-zero when any named budget is exceeded or its
// artifact is missing.
//
//   node tools/size-budgets.mjs android-toolkit | metal-toolkit | wasm-toolkit ...
//
// Env: ANDROID_NDK_HOME (for llvm-strip) or LLVM_STRIP.
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, readdirSync, statSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { gzipSync } from "node:zlib";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const MB = 1024 * 1024;

/** @typedef {{files: string[], measure: (files: string[]) => number, limit: number}} Budget */

// The whole toolkit a GPU app links: toolkit, host, paint with its GPU
// encoder, the GPU backend, the shell and image decoding. CPU and embed
// variants are alternatives to these, so they are not counted.
/** @param {string} preset @param {string[]} platform package-relative archives */
function toolkit(preset, platform) {
  return [
    "glim-core/libglim-core.a",
    "glim-image/libglim-image.a",
    "glim-paint/libglim-paint.a",
    "glim-paint/libglim-paint-gpu.a",
    "glim-toolkit/libglim-toolkit.a",
    "glim-toolkit-host/libglim-toolkit-host.a",
    ...platform,
  ].map((f) => `build/${preset}/packages/${f}`);
}

/** @param {string[]} files */
const total = (files) => files.reduce((sum, f) => sum + statSync(f).size, 0);

/** @type {Record<string, Budget>} */
const budgets = {
  // Debug info stripped (release archives still carry it on the NDK).
  "android-toolkit": {
    files: toolkit("android-arm64-vulkan-release", [
      "glim-gpu-vulkan/libglim-gpu-vulkan.a",
      "glim-gpu-vulkan/libglim-volk.a",
      "glim-shell-android/libglim-shell.a",
      "glim-shell-android/libglim-android-glue.a",
    ]),
    measure: (files) => files.reduce((sum, f) => sum + stripped(f), 0),
    limit: 6 * MB,
  },
  "metal-toolkit": {
    files: toolkit("macos-metal-release", [
      "glim-gpu-metal/libglim-gpu-metal.a",
      "glim-shell-macos/libglim-shell.a",
    ]),
    measure: total,
    limit: 4 * MB,
  },
  // What a browser downloads scales with the gzipped code.
  "wasm-toolkit": {
    files: toolkit("web-webgpu-release", [
      "glim-gpu-webgpu/libglim-gpu-webgpu.a",
      "glim-shell-web/libglim-shell.a",
    ]),
    measure: (files) => gzipSync(Buffer.concat(files.map((f) => readFileSync(f))), { level: 9 }).length,
    limit: 1 * MB,
  },
};

/** @param {string} file */
function stripped(file) {
  const out = path.join(mkdtempSync(path.join(os.tmpdir(), "glim-size-")), "stripped");
  execFileSync(llvmStrip(), ["--strip-debug", "-o", out, file]);
  return statSync(out).size;
}

function llvmStrip() {
  if (process.env.LLVM_STRIP) return process.env.LLVM_STRIP;
  const ndk = process.env.ANDROID_NDK_HOME;
  if (!ndk) throw new Error("set ANDROID_NDK_HOME or LLVM_STRIP");
  const prebuilt = path.join(ndk, "toolchains/llvm/prebuilt");
  return path.join(prebuilt, readdirSync(prebuilt)[0], "bin/llvm-strip");
}

const names = process.argv.slice(2);
if (!names.length || names.some((n) => !(n in budgets))) {
  console.error(`usage: size-budgets.mjs <${Object.keys(budgets).join("|")}>...`);
  process.exit(2);
}
let failed = false;
for (const name of names) {
  const budget = budgets[name];
  const files = budget.files.map((f) => path.join(root, f));
  const missing = files.filter((f) => !existsSync(f));
  if (missing.length) {
    console.error(`${name}: FAIL missing ${missing.map((f) => path.relative(root, f)).join(", ")}`);
    failed = true;
    continue;
  }
  const size = budget.measure(files);
  const ok = size <= budget.limit;
  const fmt = (/** @type {number} */ n) => `${(n / MB).toFixed(2)} MB`;
  console.log(`${name}: ${ok ? "ok" : "FAIL"} ${fmt(size)} (limit ${fmt(budget.limit)})`);
  failed ||= !ok;
}
process.exit(failed ? 1 : 0);
