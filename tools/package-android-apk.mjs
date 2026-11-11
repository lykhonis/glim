// @ts-check
// Package a NativeActivity APK from a built .so with the SDK build-tools.
//
//   node tools/package-android-apk.mjs <lib.so> <AndroidManifest.xml> <res dir> <out.apk> <sdk> [abi]
//
// Missing build-tools or android.jar skip packaging (exit 0) so a bare NDK
// build still succeeds.
import { spawnSync } from "node:child_process";
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import os from "node:os";
import path from "node:path";
import { crc32 } from "node:zlib";

const [lib, manifest, res, out, sdk, abi = "arm64-v8a"] = process.argv.slice(2);
const platforms = ["android-35", "android-34", "android-33", "android-32", "android-31", "android-30"];

/** @param {string} message @returns {never} */
function fail(message) {
  console.error(message);
  process.exit(1);
}

// Missing SDK pieces skip locally but fail in CI, where the APK must exist.
/** @param {string} message @returns {never} */
function skip(message) {
  if (process.env.CI) fail(`${message}; APK required in CI`);
  console.error(`${message}; skipping APK`);
  process.exit(0);
}

function run(/** @type {string} */ bin, /** @type {string[]} */ args) {
  const r = spawnSync(bin, args, { stdio: ["ignore", "ignore", "inherit"] });
  if (r.error) fail(`${bin}: ${r.error.message}`);
  if (r.status !== 0) fail(`${path.basename(bin)} failed (exit ${r.status})`);
}

/** Highest build-tools version, compared numerically (35.0.1 < 36.0.0). */
function latestBuildTools() {
  const dir = path.join(sdk, "build-tools");
  if (!existsSync(dir)) return undefined;
  const versions = readdirSync(dir).sort((a, b) =>
    a.localeCompare(b, undefined, { numeric: true }),
  );
  return versions.length ? path.join(dir, versions[versions.length - 1]) : undefined;
}

function filesUnder(/** @type {string} */ dir) {
  /** @type {string[]} */
  const found = [];
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) found.push(...filesUnder(p));
    else found.push(p);
  }
  return found.sort();
}

/**
 * Appends one STORED (uncompressed) entry to a zip, rewriting the central
 * directory. Stored keeps the .so page-alignable by zipalign, and leaves
 * aapt2's uncompressed resources.arsc untouched (API 30+ rejects it
 * compressed).
 * @param {string} zipPath
 * @param {string} name
 * @param {Buffer} data
 */
function appendStored(zipPath, name, data) {
  const zip = readFileSync(zipPath);
  const eocd = zip.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  if (eocd < 0) fail(`${zipPath}: not a zip`);
  const entries = zip.readUInt16LE(eocd + 10);
  const cdSize = zip.readUInt32LE(eocd + 12);
  const cdOffset = zip.readUInt32LE(eocd + 16);
  const nameBytes = Buffer.from(name, "utf8");
  const crc = crc32(data);

  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(10, 4); // version needed
  local.writeUInt16LE(0x0800, 6); // UTF-8 name
  local.writeUInt16LE(0, 8); // stored
  local.writeUInt32LE(0, 10); // dos time/date: fixed for reproducible output
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(data.length, 18);
  local.writeUInt32LE(data.length, 22);
  local.writeUInt16LE(nameBytes.length, 26);
  local.writeUInt16LE(0, 28);

  const central = Buffer.alloc(46);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt16LE(20, 4); // version made by
  central.writeUInt16LE(10, 6);
  central.writeUInt16LE(0x0800, 8);
  central.writeUInt16LE(0, 10);
  central.writeUInt32LE(0, 12);
  central.writeUInt32LE(crc, 16);
  central.writeUInt32LE(data.length, 20);
  central.writeUInt32LE(data.length, 24);
  central.writeUInt16LE(nameBytes.length, 28);
  central.writeUInt32LE(cdOffset, 42); // new entry sits where the old CD began

  const newCdOffset = cdOffset + local.length + nameBytes.length + data.length;
  const end = Buffer.from(zip.subarray(eocd));
  end.writeUInt16LE(entries + 1, 8);
  end.writeUInt16LE(entries + 1, 10);
  end.writeUInt32LE(cdSize + central.length + nameBytes.length, 12);
  end.writeUInt32LE(newCdOffset, 16);

  writeFileSync(
    zipPath,
    Buffer.concat([
      zip.subarray(0, cdOffset),
      local,
      nameBytes,
      data,
      zip.subarray(cdOffset, cdOffset + cdSize),
      central,
      nameBytes,
      end,
    ]),
  );
}

if (!out || !sdk) fail("usage: package-android-apk.mjs <lib.so> <manifest> <res> <out.apk> <sdk> [abi]");
for (const [what, p] of [["library", lib], ["manifest", manifest], ["res dir", res]]) {
  if (!existsSync(p)) fail(`missing ${what}: ${p}`);
}

const bt = latestBuildTools();
if (!bt) skip(`no Android build-tools in ${sdk}/build-tools`);
const aapt2 = path.join(bt, "aapt2");
const zipalign = path.join(bt, "zipalign");
const apksigner = path.join(bt, "apksigner");
if (![aapt2, zipalign, apksigner].every(existsSync)) skip(`${bt} lacks aapt2/zipalign/apksigner`);
const androidJar = platforms
  .map((api) => path.join(sdk, "platforms", api, "android.jar"))
  .find(existsSync);
if (!androidJar) skip(`no android.jar in ${sdk}/platforms`);

const work = mkdtempSync(path.join(os.tmpdir(), "glim-apk-"));
try {
  const compiled = path.join(work, "compiled");
  mkdirSync(compiled);
  run(aapt2, ["compile", "--dir", res, "-o", compiled]);
  const flats = filesUnder(compiled).filter((f) => f.endsWith(".flat"));
  if (!flats.length) fail("aapt2 compile produced no resources");

  const unsigned = path.join(work, "unsigned.apk");
  run(aapt2, ["link", "-o", unsigned, "-I", androidJar, "--manifest", manifest, "--auto-add-overlay", ...flats]);
  appendStored(unsigned, `lib/${abi}/${path.basename(lib)}`, readFileSync(lib));

  const aligned = path.join(work, "aligned.apk");
  run(zipalign, ["-f", "-p", "4", unsigned, aligned]);

  const keystore = path.join(os.homedir(), ".android", "debug.keystore");
  if (!existsSync(keystore)) {
    mkdirSync(path.dirname(keystore), { recursive: true });
    run("keytool", [
      "-genkeypair", "-keystore", keystore, "-storepass", "android", "-keypass", "android",
      "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
      "-dname", "CN=Android Debug,O=Android,C=US",
    ]);
  }

  mkdirSync(path.dirname(out), { recursive: true });
  run(apksigner, [
    "sign", "--ks", keystore, "--ks-pass", "pass:android", "--key-pass", "pass:android",
    "--ks-key-alias", "androiddebugkey", "--out", out, aligned,
  ]);
  run(zipalign, ["-c", "-p", "4", out]);
  console.log(`APK ${out}`);
} finally {
  rmSync(work, { recursive: true, force: true });
}
