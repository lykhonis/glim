// @ts-check
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";

/** @param {string} message @returns {never} */
function fail(message) {
  console.error(message);
  process.exit(2);
}

const args = process.argv.slice(2);
const split = args.indexOf("--");
const own = split < 0 ? args : args.slice(0, split);
const appArgs = split < 0 ? [] : args.slice(split + 1);
/** @param {string} name */
const option = (name) => {
  const i = own.indexOf(name);
  if (i < 0) return undefined;
  const value = own[i + 1];
  if (value === undefined) fail(`${name} needs a value`);
  own.splice(i, 2);
  return value;
};
/** @param {string} name */
const flag = (name) => {
  const i = own.indexOf(name);
  if (i >= 0) own.splice(i, 1);
  return i >= 0;
};

const runs = Number(option("--runs") ?? 7);
const budget = option("--budget");
const base = option("--base");
const cold = flag("--cold");
const app = own[0];
if (!app || own.length > 1 || !(runs > 0)) {
  fail("usage: node tools/startup.mjs <app> [--runs N] [--cold] [--budget ms] [--base <app>] [-- app args]");
}
for (const bin of [app, base]) if (bin && !existsSync(bin)) fail(`missing ${bin}`);

// A sleeping display delivers no vsync and presents nothing, so macOS
// runs hold a user-activity assertion that keeps it awake.
/** @param {string} bin @param {string[]} argv @returns {[string, string[]]} */
const awake = (bin, argv) =>
  process.platform === "darwin" ? ["caffeinate", ["-u", bin, ...argv]] : [bin, argv];

/** One run's phases in order, ms each. @param {string} bin @returns {Map<string, number>} */
function sample(bin) {
  const env = cold ? { ...process.env, GLIM_METAL_COLD_SHADERS: "1" } : process.env;
  let out = "";
  try {
    out = execFileSync(...awake(bin, ["--startup", ...appArgs]), { env, encoding: "utf8", timeout: 60000, stdio: ["ignore", "pipe", "pipe"] });
  } catch (e) {
    const { message, stderr } = /** @type {Error & { stderr?: string }} */ (e);
    fail(`${bin} --startup failed: ${message}\n${stderr ?? ""}`);
  }
  const phases = new Map();
  for (const m of out.matchAll(/^startup (\S+)\s+([\d.]+)$/gm)) phases.set(m[1], Number(m[2]));
  if (!phases.has("total")) fail(`${bin} printed no startup report (is --startup supported?)\n${out}`);
  return phases;
}

/** @param {number[]} v */
const median = (v) => {
  const s = [...v].sort((a, b) => a - b);
  const mid = Math.floor(s.length / 2);
  return s.length % 2 ? s[mid] : (s[mid - 1] + s[mid]) / 2;
};

/** @type {Map<string, number[]>[]} */
const samples = [new Map(), new Map()];
const bins = base ? [app, base] : [app];
for (let r = 0; r < runs; ++r) {
  bins.forEach((bin, side) => {
    for (const [phase, ms] of sample(bin)) {
      const list = samples[side].get(phase) ?? [];
      list.push(ms);
      samples[side].set(phase, list);
    }
  });
}

const pad = (/** @type {string} */ s, /** @type {number} */ n) => s.padStart(n);
const ms = (/** @type {number | undefined} */ v) => (v === undefined ? "-" : v.toFixed(1));
console.log(`startup: ${app}${cold ? " (cold shaders)" : ""}, median of ${runs}`);
console.log(`  ${"phase".padEnd(12)}${pad("ms", 9)}${pad("min", 9)}${pad("max", 9)}${base ? pad("base", 9) + pad("delta", 9) : ""}`);
for (const [phase, values] of samples[0]) {
  const here = median(values);
  const there = samples[1].get(phase);
  const row = [pad(ms(here), 9), pad(ms(Math.min(...values)), 9), pad(ms(Math.max(...values)), 9)];
  if (base) row.push(pad(ms(there && median(there)), 9), pad(there ? ms(here - median(there)) : "-", 9));
  console.log(`  ${phase.padEnd(12)}${row.join("")}`);
}

const total = median(samples[0].get("total") ?? []);
if (budget !== undefined && total > Number(budget)) {
  console.error(`startup: median total ${total.toFixed(1)} ms is over the ${budget} ms budget`);
  process.exit(1);
}
