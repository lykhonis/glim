// @ts-check
import { execFileSync } from "node:child_process";
import { mkdirSync, rmSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { stackArchives } from "./sdk-stack.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const work = path.join(root, "build", "apple");
const output = path.join(root, "apple", "Glim.xcframework");

/** @type {Record<string, {preset: string, archs: string}>} */
const slices = {
  macos: { preset: "macos-metal-release", archs: "arm64;x86_64" },
  ios: { preset: "ios-metal-debug", archs: "arm64" },
  "ios-sim": { preset: "ios-sim-metal-debug", archs: "arm64;x86_64" },
  tvos: { preset: "tvos-metal-debug", archs: "arm64" },
  "tvos-sim": { preset: "tvos-sim-metal-debug", archs: "arm64;x86_64" },
};

const archives = stackArchives(["glim-gpu-metal"]);

/** @param {string} cmd @param {string[]} args */
function run(cmd, args) {
  console.log(`$ ${cmd} ${args.join(" ")}`);
  execFileSync(cmd, args, { cwd: root, stdio: "inherit" });
}

const names = process.argv.slice(2);
for (const name of names) {
  if (!slices[name]) throw new Error(`unknown slice ${name}; have ${Object.keys(slices).join(", ")}`);
}

const frameworkArgs = [];
for (const name of names.length ? names : Object.keys(slices)) {
  const { preset, archs } = slices[name];
  const build = path.join(work, name, "build");
  const prefix = path.join(work, name, "prefix");
  rmSync(prefix, { recursive: true, force: true });
  run("cmake", [
    "--preset", preset,
    "-B", build,
    "-DCMAKE_BUILD_TYPE=Release",
    `-DCMAKE_OSX_ARCHITECTURES=${archs}`,
  ]);
  run("cmake", ["--build", build, "--target", "glim-package"]);
  run("cmake", ["--install", build, "--component", "glim", "--prefix", prefix]);
  const merged = path.join(work, name, "libGlim.a");
  run("libtool", ["-static", "-no_warning_for_no_symbols", "-o", merged,
    ...archives.map((a) => path.join(prefix, "lib", `lib${a}.a`))]);
  frameworkArgs.push("-library", merged, "-headers", path.join(prefix, "include"));
}

rmSync(output, { recursive: true, force: true });
mkdirSync(path.dirname(output), { recursive: true });
run("xcodebuild", ["-create-xcframework", ...frameworkArgs, "-output", output]);
