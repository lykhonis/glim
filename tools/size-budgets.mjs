// @ts-check
// Size budgets (docs decisions.md § Budgets, hardening H5) measured on
// release builds. Exits non-zero when any named budget is exceeded or its
// artifact is missing.
//
//   node tools/size-budgets.mjs android-so | metal-static | wasm-gzip ...
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

/** @type {Record<string, Budget>} */
const budgets = {
  // Embedded .so stripped, no fonts: CI fail > 3 MB (hard ceiling 5 MB).
  "android-so": {
    files: ["build/android-arm64-vulkan-release/examples/hello/libglim-hello.so"],
    measure: stripped,
    limit: 3 * MB,
  },
  // Static glim::glim Metal slice (core + paint + paint-gpu + gpu-metal + shell), no fonts.
  "metal-static": {
    files: [
      "glim-core/libglim-core.a",
      "glim-paint/libglim-paint.a",
      "glim-paint/libglim-paint-gpu.a",
      "glim-gpu-metal/libglim-gpu-metal.a",
      "glim-shell-macos/libglim-shell.a",
    ].map((f) => `build/macos-metal-release/packages/${f}`),
    measure: (files) => files.reduce((sum, f) => sum + statSync(f).size, 0),
    limit: 2 * MB,
  },
  "wasm-gzip": {
    files: ["build/web-webgpu-release/examples/hello/glim-hello.wasm"],
    measure: (files) => gzipSync(readFileSync(files[0]), { level: 9 }).length,
    limit: 1 * MB,
  },
};

/** @param {string[]} files */
function stripped(files) {
  const out = path.join(mkdtempSync(path.join(os.tmpdir(), "glim-size-")), "stripped");
  execFileSync(llvmStrip(), ["-o", out, files[0]]);
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
