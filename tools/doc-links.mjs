// @ts-check
// Checks relative links and #anchors in local markdown (docs/ is gitignored,
// so this is a local check, not a CI gate). Exits non-zero on any broken link.
//
//   node tools/doc-links.mjs [dir]   (default: docs)
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import path from "node:path";

const root = path.resolve(process.argv[2] ?? "docs");
if (!existsSync(root)) {
  console.error(`missing ${root}`);
  process.exit(1);
}

/** @param {string} dir @returns {string[]} */
function markdownUnder(dir) {
  return readdirSync(dir).flatMap((name) => {
    const p = path.join(dir, name);
    if (statSync(p).isDirectory()) return markdownUnder(p);
    return p.endsWith(".md") ? [p] : [];
  });
}

/** GitHub-style heading slug. @param {string} heading */
function slug(heading) {
  return heading
    .trim()
    .toLowerCase()
    .replace(/[`*_~]/g, "")
    .replace(/\[([^\]]*)\]\([^)]*\)/g, "$1")
    .replace(/[^\p{L}\p{N} -]/gu, "")
    .replace(/ /g, "-");
}

/** @type {Map<string, Set<string>>} */
const anchorCache = new Map();
/** @param {string} file */
function anchors(file) {
  let set = anchorCache.get(file);
  if (set) return set;
  set = new Set();
  /** @type {Map<string, number>} */
  const seen = new Map();
  let fenced = false;
  for (const line of readFileSync(file, "utf8").split("\n")) {
    if (/^\s*```/.test(line)) fenced = !fenced;
    const m = !fenced && /^#{1,6}\s+(.*?)\s*#*\s*$/.exec(line);
    if (!m) continue;
    const base = slug(m[1]);
    const n = seen.get(base) ?? 0;
    seen.set(base, n + 1);
    set.add(n ? `${base}-${n}` : base);
  }
  anchorCache.set(file, set);
  return set;
}

const repo = path.dirname(root);
let broken = 0;
let checked = 0;
for (const file of markdownUnder(root)) {
  const text = readFileSync(file, "utf8").replace(/```[\s\S]*?```/g, "").replace(/`[^`\n]*`/g, "");
  for (const [, target] of text.matchAll(/\]\(\s*<?([^)\s>]+)>?(?:\s+"[^"]*")?\s*\)/g)) {
    if (/^[a-z][a-z0-9+.-]*:/i.test(target)) continue;
    checked++;
    const [ref, hash] = target.split("#");
    let dest = ref ? path.resolve(path.dirname(file), decodeURIComponent(ref)) : file;
    // Repo-rooted links (/packages/...) resolve against the checkout.
    if (ref.startsWith("/")) dest = path.join(repo, ref);
    const where = path.relative(repo, file);
    if (!existsSync(dest)) {
      console.error(`${where}: missing ${target}`);
      broken++;
    } else if (hash && dest.endsWith(".md") && !anchors(dest).has(hash)) {
      console.error(`${where}: no anchor #${hash} in ${path.relative(repo, dest)}`);
      broken++;
    }
  }
}
console.log(`checked ${checked} links, ${broken} broken`);
process.exit(broken ? 1 : 0);
