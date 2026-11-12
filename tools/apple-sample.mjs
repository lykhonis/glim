// @ts-check
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const derived = path.join(root, "build", "apple", "sample");
const destinations = [
  "generic/platform=macOS",
  "generic/platform=iOS",
  "generic/platform=iOS Simulator",
  "generic/platform=tvOS",
  "generic/platform=tvOS Simulator",
];

if (!existsSync(path.join(root, "apple", "Glim.xcframework"))) {
  throw new Error("apple/Glim.xcframework is missing; run pnpm xcframework");
}

/** @param {string} cmd @param {string[]} args */
function run(cmd, args) {
  console.log(`$ ${cmd} ${args.join(" ")}`);
  execFileSync(cmd, args, { cwd: root, stdio: "inherit" });
}

for (const destination of destinations) {
  run("xcodebuild", [
    "-quiet",
    "-project", "apple/Sample/GlimSample.xcodeproj",
    "-scheme", "GlimSample",
    "-configuration", "Release",
    "-destination", destination,
    "-derivedDataPath", derived,
    "GCC_TREAT_WARNINGS_AS_ERRORS=YES",
    "build",
  ]);
}
run(path.join(derived, "Build/Products/Release/GlimSample.app/Contents/MacOS/GlimSample"), ["--help"]);
