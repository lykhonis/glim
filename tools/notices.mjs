// @ts-check
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const noticesFile = "THIRD_PARTY_NOTICES.md";

/** @param {string} source */
function stbLicense(source) {
  const start = source.lastIndexOf("This software is available under 2 licenses");
  const end = source.indexOf("*/", start);
  if (start < 0 || end < 0) throw new Error("stb license block not found");
  return source.slice(start, end).replace(/-{20,}\n/g, "").trim();
}

/** @param {string} root */
function components(root) {
  const read = (/** @type {string} */ file) => readFileSync(path.join(root, file), "utf8").trim();
  return [
    { name: "Inter", use: "embedded UI font", license: read("vendor/inter/OFL.txt") },
    { name: "stb_image, stb_truetype", use: "image decode and glyph rasterization", license: stbLicense(read("vendor/stb/stb_image.h")) },
    { name: "volk", use: "Vulkan loader (Android and Linux builds)", license: read("vendor/volk/LICENSE.md") },
    { name: "WebAssembly Micro Runtime", use: "wasm guest runtime (GLIM_GUEST builds)", license: read("vendor/wamr/LICENSE") },
  ];
}

/** @param {string} root */
export function notices(root) {
  const sections = components(root).map(({ name, use, license }) => `## ${name}\n\nUsed for: ${use}.\n\n\`\`\`text\n${license}\n\`\`\`\n`);
  return `# Third-party notices\n\nGlim includes the following third-party software.\n\n${sections.join("\n")}`;
}

/** @param {string} root */
export function checkNotices(root) {
  const file = path.join(root, noticesFile);
  if (!existsSync(file)) throw new Error(`missing ${noticesFile}`);
  if (readFileSync(file, "utf8") !== notices(root)) throw new Error(`${noticesFile} drifted; run node tools/notices.mjs --write`);
}

/** @param {string} listing @param {string} bundle */
export function requireNotices(listing, bundle) {
  if (!listing.includes(noticesFile)) throw new Error(`${bundle} missing ${noticesFile}`);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
  if (process.argv.includes("--write")) writeFileSync(path.join(root, noticesFile), notices(root));
  else checkNotices(root);
}
