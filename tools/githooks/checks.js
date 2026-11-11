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

function formatter() {
  return fs.existsSync(path.join(root, "node_modules/@wasm-fmt/clang-format"));
}

/** @param {string[]} files */
function format(files = []) {
  if (!formatter()) {
    console.error("run pnpm install for the formatter; skipping format check");
    return;
  }
  step("format (run pnpm format)", "node", ["tools/format.mjs", "--check", ...files]);
}

function lint() {
  step("glim-lint configure", "cmake", ["-S", "tools/glim-lint", "-B", "build/glim-lint", "-DCMAKE_BUILD_TYPE=Release"]);
  step("glim-lint build", "cmake", ["--build", "build/glim-lint"]);
  step("glim-lint", "build/glim-lint/glim-lint", ["--root", "."]);
}

function lintTests() {
  step("glim-lint tests", "ctest", ["--test-dir", "build/glim-lint", "--output-on-failure"]);
}

// Host debug build + tests; skipped where no preset is configured.
function hostTests() {
  const preset = process.platform === "darwin" ? "macos-metal-debug" : "linux-vulkan-debug";
  const dir = path.join("build", preset);
  if (!fs.existsSync(path.join(root, dir, "CMakeCache.txt"))) {
    console.error(`${dir} not configured; skipping build and tests`);
    return;
  }
  step(`build ${preset}`, "cmake", ["--build", dir, "-j", "8"]);
  step(`test ${preset}`, "ctest", ["--test-dir", dir, "-j", "6", "--timeout", "900", "--output-on-failure"]);
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

module.exports = { format, lint, lintTests, hostTests, stagedCpp };
