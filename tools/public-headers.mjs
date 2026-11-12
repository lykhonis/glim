// @ts-check
import { existsSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const manifestPath = "cmake/public-headers.txt";

/** @param {string} dir @param {string} rel @param {string[]} out */
function walk(dir, rel, out) {
  for (const name of readdirSync(dir).sort()) {
    if (name === ".DS_Store") continue;
    const abs = path.join(dir, name);
    const next = `${rel}/${name}`;
    if (statSync(abs).isDirectory()) walk(abs, next, out);
    else out.push(next);
  }
}

/** @param {string} root */
export function publicHeaders(root) {
  /** @type {string[]} */
  const out = [];
  const packages = path.join(root, "packages");
  for (const name of readdirSync(packages).sort()) {
    const include = path.join(packages, name, "include");
    if (!existsSync(include)) continue;
    walk(include, `packages/${name}/include`, out);
  }
  return out;
}

/** @param {string} root */
export function checkPublicHeaders(root) {
  const file = path.join(root, manifestPath);
  const actual = `${publicHeaders(root).join("\n")}\n`;
  if (!existsSync(file)) throw new Error(`missing ${manifestPath}`);
  const expected = readFileSync(file, "utf8");
  if (expected === actual) return;
  const have = new Set(expected.trim().split("\n").filter(Boolean));
  const want = new Set(actual.trim().split("\n").filter(Boolean));
  const added = [...want].filter((item) => !have.has(item));
  const removed = [...have].filter((item) => !want.has(item));
  throw new Error(`${manifestPath} drifted\nadded:\n${added.join("\n")}\nremoved:\n${removed.join("\n")}`);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
  const file = path.join(root, manifestPath);
  if (process.argv.includes("--write")) writeFileSync(file, `${publicHeaders(root).join("\n")}\n`);
  else checkPublicHeaders(root);
}
