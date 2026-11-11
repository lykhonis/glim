// @ts-check
// Line and function coverage of packages/ from the host test suite, gated
// by the floors in tools/coverage-baseline.json.
//
//   node tools/coverage.mjs            build, test, report, fail below a floor
//   node tools/coverage.mjs --update   same, then raise floors (never lower)
//
// Needs gcovr (brew install gcovr / pipx install gcovr).
import { execFileSync } from "node:child_process";
import { appendFileSync, existsSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const baselinePath = path.join(root, "tools/coverage-baseline.json");
const checks = createRequire(import.meta.url)("./githooks/checks.js");

/** @typedef {{lines: number, functions: number}} Percent */
/** @typedef {{total: Percent, packages: Record<string, number>}} Floors */
/** @typedef {{lc: number, lt: number, fc: number, ft: number}} Counts */

// Timing-dependent test paths flip single lines between runs.
const slack = 0.1;

/** @param {number} value @param {number} floor */
const below = (value, floor) => Math.round((floor - value) * 10) > Math.round(slack * 10);

/** @param {number} covered @param {number} total */
const percent = (covered, total) => (total === 0 ? 100 : Math.floor((1000 * covered) / total) / 10);

/** @param {string} preset @returns {Record<string, Counts>} */
function measure(preset) {
  const gcov = process.platform === "darwin" ? ["--gcov-executable", "xcrun llvm-cov gcov"] : [];
  const json = execFileSync(
    "gcovr",
    [
      "--root", root, path.join(root, "build", preset), ...gcov,
      "--merge-mode-functions=separate", "--merge-lines", "-j", "8",
      "--filter", "packages/", "--exclude", ".*/tests/.*",
      "--json-summary", "-",
    ],
    { cwd: root, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 },
  );
  /** @type {{files: {filename: string, line_covered: number, line_total: number, function_covered: number, function_total: number}[]}} */
  const summary = JSON.parse(json);
  /** @type {Record<string, Counts>} */
  const byPackage = {};
  for (const f of summary.files) {
    const name = f.filename.split("/")[1];
    const c = (byPackage[name] ??= { lc: 0, lt: 0, fc: 0, ft: 0 });
    c.lc += f.line_covered;
    c.lt += f.line_total;
    c.fc += f.function_covered;
    c.ft += f.function_total;
  }
  return byPackage;
}

/** @param {Record<string, Counts>} byPackage @returns {Floors} */
function floorsOf(byPackage) {
  const sum = Object.values(byPackage).reduce(
    (a, c) => ({ lc: a.lc + c.lc, lt: a.lt + c.lt, fc: a.fc + c.fc, ft: a.ft + c.ft }),
    { lc: 0, lt: 0, fc: 0, ft: 0 },
  );
  return {
    total: { lines: percent(sum.lc, sum.lt), functions: percent(sum.fc, sum.ft) },
    packages: Object.fromEntries(
      Object.entries(byPackage)
        .sort(([a], [b]) => a.localeCompare(b))
        .map(([name, c]) => [name, percent(c.lc, c.lt)]),
    ),
  };
}

/** @param {Floors} got @param {Floors} floor @returns {string[]} */
function regressions(got, floor) {
  const out = [];
  for (const key of /** @type {const} */ (["lines", "functions"])) {
    if (below(got.total[key], floor.total[key])) out.push(`total ${key} ${got.total[key]}% < ${floor.total[key]}%`);
  }
  for (const [name, min] of Object.entries(floor.packages)) {
    const value = got.packages[name];
    if (value === undefined) out.push(`${name}: no coverage data`);
    else if (below(value, min)) out.push(`${name} lines ${value}% < ${min}%`);
  }
  return out;
}

/** @param {Floors} got @param {Floors} floor @returns {Floors} */
function ratchet(got, floor) {
  return {
    total: {
      lines: Math.max(got.total.lines, floor.total.lines),
      functions: Math.max(got.total.functions, floor.total.functions),
    },
    packages: Object.fromEntries(
      Object.entries(got.packages).map(([name, v]) => [name, Math.max(v, floor.packages[name] ?? 0)]),
    ),
  };
}

/** @param {string} host @param {Floors} got @param {Floors} floor */
function table(host, got, floor) {
  const row = (/** @type {string} */ name, /** @type {number} */ v, /** @type {number | undefined} */ min) =>
    `| ${name} | ${v}% | ${min === undefined ? "new" : `${min}%`} | ${min !== undefined && below(v, min) ? "❌" : "✅"} |`;
  return [
    `### Coverage (${host})`,
    "",
    "| | Measured | Floor | |",
    "| --- | ---: | ---: | --- |",
    row("**lines**", got.total.lines, floor.total.lines),
    row("**functions**", got.total.functions, floor.total.functions),
    ...Object.entries(got.packages).map(([name, v]) => row(name, v, floor.packages[name])),
    "",
  ].join("\n");
}

const update = process.argv.includes("--update");
const preset = checks.hostPreset("coverage");
const host = preset.replace(/-coverage$/, "");
/** @type {Record<string, Floors>} */
const baseline = JSON.parse(readFileSync(baselinePath, "utf8"));
const floor = baseline[host];
if (!floor) {
  console.error(`no coverage floors for ${host} in tools/coverage-baseline.json`);
  process.exit(1);
}

const buildDir = path.join(root, "build", preset);
const stale = existsSync(buildDir) ? readdirSync(buildDir, { recursive: true, encoding: "utf8" }) : [];
for (const f of stale) {
  if (f.endsWith(".gcda")) rmSync(path.join(buildDir, f));
}
checks.hostTests(preset);
const got = floorsOf(measure(preset));
const report = table(host, got, floor);
console.log(report);
if (process.env.GITHUB_STEP_SUMMARY) appendFileSync(process.env.GITHUB_STEP_SUMMARY, report);

const failed = regressions(got, floor);
if (failed.length) {
  console.error(`coverage below the floor:\n  ${failed.join("\n  ")}`);
  process.exit(1);
}

if (update) {
  const raised = ratchet(got, floor);
  baseline[host] = raised;
  writeFileSync(baselinePath, `${JSON.stringify(baseline, null, 2)}\n`);
  console.log("floors raised to the measured values");
}
