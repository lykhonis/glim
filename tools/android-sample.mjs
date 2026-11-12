// @ts-check
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const sample = path.join(root, "android", "sample");
const apk = path.join(sample, "build", "outputs", "apk", "debug", "glim-sample-debug.apk");
const abis = ["arm64-v8a", "x86_64"];

if (!existsSync(path.join(root, "android", "glim.aar"))) {
  throw new Error("android/glim.aar is missing; run pnpm aar");
}

console.log("$ ./gradlew assembleDebug");
execFileSync(path.join(sample, "gradlew"), ["--no-daemon", "assembleDebug"], { cwd: sample, stdio: "inherit" });

const entries = execFileSync("unzip", ["-l", apk], { encoding: "utf8" });
for (const abi of abis) {
  if (!entries.includes(`lib/${abi}/libglim-sample.so`)) throw new Error(`${apk} has no ${abi} libglim-sample.so`);
}
console.log(`glim android sample ok (${abis.join(", ")})`);
