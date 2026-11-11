// @ts-check
// Format C++ with the pinned WASM clang-format (host independent).
//   node tools/format.mjs            format every tracked file in place
//   node tools/format.mjs --check    exit 1 listing files that differ
//   node tools/format.mjs [--check] FILE...   limit to the given files
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import init, { format } from "@wasm-fmt/clang-format";

const root = execFileSync("git", ["rev-parse", "--show-toplevel"], {
  encoding: "utf8",
}).trim();

/** @type {Map<string, string>} */
const styles = new Map();

/** Nearest .clang-format above the file wins, like clang-format itself. */
function styleFor(/** @type {string} */ file) {
  let dir = path.dirname(file);
  for (;;) {
    const hit = styles.get(dir);
    if (hit !== undefined) return hit;
    const candidate = path.join(dir, ".clang-format");
    if (existsSync(candidate)) {
      const text = readFileSync(candidate, "utf8");
      styles.set(dir, text);
      return text;
    }
    if (dir === root || dir === path.dirname(dir)) {
      throw new Error(`no .clang-format above ${file}`);
    }
    dir = path.dirname(dir);
  }
}

function trackedSources() {
  return execFileSync(
    "git",
    ["ls-files", "*.cpp", "*.h", ":!:vendor/**"],
    { cwd: root, encoding: "utf8" },
  )
    .split("\n")
    .filter((f) => f && existsSync(path.join(root, f)));
}

const args = process.argv.slice(2);
const check = args.includes("--check");
const named = args.filter((a) => !a.startsWith("--"));
const files = (named.length ? named : trackedSources()).map((f) =>
  path.resolve(root, f),
);

await init();

/** @type {string[]} */
const dirty = [];
for (const file of files) {
  const source = readFileSync(file, "utf8");
  const formatted = format(source, file, styleFor(file));
  if (formatted === source) continue;
  dirty.push(path.relative(root, file));
  if (!check) writeFileSync(file, formatted);
}

if (check && dirty.length) {
  console.error(`not formatted:\n  ${dirty.join("\n  ")}`);
  console.error("run: pnpm format");
  process.exit(1);
}
console.log(
  `${check ? "checked" : "formatted"} ${files.length} files, ${dirty.length} ${check ? "differ" : "changed"}`,
);
