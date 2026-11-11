// @ts-check
// Checks shared by the git hooks: light set on commit, heavier set on push.
const { execFileSync, spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const root = execFileSync("git", ["rev-parse", "--show-toplevel"], { encoding: "utf8" }).trim();

/** @param {string} name @param {string} cmd @param {string[]} args */
function step(name, cmd, args) {
  const r = spawnSync(cmd, args, { cwd: root, stdio: "inherit" });
  if (r.status !== 0) {
    console.error(`\n${name} failed`);
    process.exit(r.status ?? 1);
  }
}

/** @param {string[]} files */
function format(files = []) {
  if (!fs.existsSync(path.join(root, "node_modules/@wasm-fmt/clang-format"))) {
    console.error("formatter missing: run pnpm install");
    process.exit(1);
  }
  step("format (run pnpm format)", "node", ["tools/format.mjs", "--check", ...files]);
}

function lint() {
  step("glim-lint configure", "cmake", ["-S", "tools/glim-lint", "-B", "build/glim-lint", "-DCMAKE_BUILD_TYPE=Release"]);
  step("glim-lint build", "cmake", ["--build", "build/glim-lint"]);
  step("glim-lint", "build/glim-lint/glim-lint", ["--root", "."]);
}

function typecheck() {
  step("typecheck (tools/*.mjs)", "pnpm", ["-s", "typecheck"]);
}

function lintTests() {
  step("glim-lint tests", "ctest", ["--test-dir", "build/glim-lint", "--output-on-failure"]);
}

// Host debug configure + full build + every registered ctest. Shared by
// pre-push and CI so no test depends on a hand-kept regex list.
/** @param {"debug" | "coverage"} variant */
function hostPreset(variant = "debug") {
  return `${process.platform === "darwin" ? "macos-metal" : "linux-vulkan"}-${variant}`;
}

function hostTests(preset = hostPreset()) {
  const dir = path.join("build", preset);
  step(`configure ${preset}`, "cmake", ["--preset", preset]);
  step(`build ${preset}`, "cmake", ["--build", dir]);
  step(`test ${preset}`, "ctest", [
    "--test-dir", dir, "-j", "6", "--timeout", "900", "--output-on-failure", "--no-tests=error",
  ]);
}

/** Staged C++ files, excluding vendor. */
function stagedCpp() {
  return execFileSync("git", ["diff", "--cached", "--name-only", "--diff-filter=ACM"], {
    cwd: root,
    encoding: "utf8",
  })
    .split("\n")
    .filter((f) => /\.(cpp|h)$/.test(f) && !f.startsWith("vendor/"));
}

module.exports = { format, lint, lintTests, typecheck, hostPreset, hostTests, stagedCpp };
