// @ts-check
import { execFileSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { requireNotices } from "./notices.mjs";
import { stackArchives } from "./sdk-stack.mjs";
import { glimVersion } from "./version.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const work = path.join(root, "build", "android-aar");
const staging = path.join(work, "aar");
const output = path.join(root, "android", "glim.aar");
const minSdk = 24;

/** @type {Record<string, string>} */
const abis = {
  "arm64-v8a": "android-arm64-vulkan-release",
  x86_64: "android-x86_64-vulkan-debug",
};

const archives = stackArchives(["glim-gpu-vulkan", "glim-volk", "glim-android-glue"]);

/** @param {string} cmd @param {string[]} args @param {string} [cwd] */
function run(cmd, args, cwd = root) {
  console.log(`$ ${cmd} ${args.join(" ")}`);
  execFileSync(cmd, args, { cwd, stdio: "inherit" });
}

function ndkRoot() {
  const env = process.env.ANDROID_NDK_HOME ?? process.env.ANDROID_NDK;
  if (env && existsSync(env)) return env;
  const sdk = process.env.ANDROID_HOME ?? path.join(os.homedir(), "Library/Android/sdk");
  const ndks = existsSync(path.join(sdk, "ndk")) ? readdirSync(path.join(sdk, "ndk")).sort().reverse() : [];
  if (!ndks.length) throw new Error("Android NDK not found; set ANDROID_NDK_HOME");
  return path.join(sdk, "ndk", ndks[0]);
}

const ndk = ndkRoot();
const ndkMajor = Number(/Pkg\.Revision\s*=\s*(\d+)/.exec(readFileSync(path.join(ndk, "source.properties"), "utf8"))?.[1]);
const prebuilt = path.join(ndk, "toolchains", "llvm", "prebuilt");
const llvmBin = path.join(prebuilt, readdirSync(prebuilt)[0], "bin");
const llvmAr = path.join(llvmBin, "llvm-ar");
const version = glimVersion(root);

rmSync(staging, { recursive: true, force: true });
const module = path.join(staging, "prefab", "modules", "glim");
mkdirSync(module, { recursive: true });

for (const [abi, preset] of Object.entries(abis)) {
  const build = path.join(work, abi, "build");
  const prefix = path.join(work, abi, "prefix");
  rmSync(prefix, { recursive: true, force: true });
  run("cmake", ["--preset", preset, "-B", build, "-DCMAKE_BUILD_TYPE=Release", `-DANDROID_ABI=${abi}`]);
  run("cmake", ["--build", build, "--target", "glim-package"]);
  run("cmake", ["--install", build, "--component", "glim", "--prefix", prefix]);
  const libs = path.join(module, "libs", `android.${abi}`);
  mkdirSync(libs, { recursive: true });
  run(llvmAr, ["qcL", path.join(libs, "libglim.a"), ...archives.map((a) => path.join(prefix, "lib", `lib${a}.a`))]);
  run(path.join(llvmBin, "llvm-strip"), ["--strip-debug", path.join(libs, "libglim.a")]);
  run(llvmAr, ["s", path.join(libs, "libglim.a")]);
  writeFileSync(path.join(libs, "abi.json"),
    JSON.stringify({ abi, api: minSdk, ndk: ndkMajor, stl: "c++_static", static: true }));
  if (!existsSync(path.join(module, "include"))) cpSync(path.join(prefix, "include"), path.join(module, "include"), { recursive: true });
  cpSync(path.join(prefix, "share", "doc", "glim"), staging, { recursive: true });
}

writeFileSync(path.join(staging, "prefab", "prefab.json"),
  JSON.stringify({ schema_version: 2, name: "glim", version, dependencies: [] }));
writeFileSync(path.join(module, "module.json"), JSON.stringify({
  export_libraries: [
    "-landroid",
    "-llog",
    "-lz",
    "-Wl,-u,ANativeActivity_onCreate",
    "-Wl,-u,android_main",
  ],
}));
writeFileSync(path.join(staging, "AndroidManifest.xml"),
  `<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.lykhonis.glim">\n` +
  `    <uses-sdk android:minSdkVersion="${minSdk}" />\n</manifest>\n`);

rmSync(output, { force: true });
mkdirSync(path.dirname(output), { recursive: true });
run("zip", ["-qr", output, "."], staging);
requireNotices(execFileSync("unzip", ["-l", output], { encoding: "utf8" }), output);
console.log(`aar written to ${output}`);
