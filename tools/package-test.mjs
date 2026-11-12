// @ts-check
// Package gate (roadmap S1): install a built preset into a fresh prefix,
// then configure, build, and run the out-of-tree tests/package sample
// against it with find_package(glim). Exits non-zero on any failure.
//
//   node tools/package-test.mjs [preset]   (default: the host debug preset)
import { execFileSync } from "node:child_process";
import { rmSync } from "node:fs";
import { createRequire } from "node:module";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const { hostPreset } = createRequire(import.meta.url)("./githooks/checks.js");

const preset = process.argv[2] ?? hostPreset();
const buildDir = path.join(root, "build", preset);
const work = path.join(root, "build", "package", preset);
const prefix = path.join(work, "prefix");
const sample = path.join(work, "sample");

/** @param {string} cmd @param {string[]} args */
function run(cmd, args) {
  console.log(`$ ${cmd} ${args.join(" ")}`);
  execFileSync(cmd, args, { cwd: root, stdio: "inherit" });
}

rmSync(work, { recursive: true, force: true });
run("cmake", ["--install", buildDir, "--component", "glim", "--prefix", prefix]);
run("cmake", [
  "-S", path.join(root, "tests", "package"),
  "-B", sample,
  "-G", "Ninja",
  `-DCMAKE_PREFIX_PATH=${prefix}`,
]);
run("cmake", ["--build", sample]);
run(path.join(sample, "glim-package-cpu"), []);
run(path.join(sample, "glim-package-host"), ["--help"]);
