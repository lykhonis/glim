// @ts-check
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { changelogSection } from "./changelog.mjs";
import { checkNotices } from "./notices.mjs";
import { checkPublicHeaders } from "./public-headers.mjs";
import { requireMatchingVersion } from "./version.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

function check() {
  const version = requireMatchingVersion(root);
  changelogSection(readFileSync(path.join(root, "CHANGELOG.md"), "utf8"), version);
  checkPublicHeaders(root);
  checkNotices(root);
  return version;
}

const version = check();
const notes = process.argv[2] === "notes";
if (!notes) {
  console.log(`glim release check ok (${version})`);
} else {
  const tagAt = process.argv.indexOf("--tag");
  const tag = tagAt >= 0 ? process.argv[tagAt + 1] : "";
  if (tag !== `v${version}`) throw new Error(`tag ${tag || "(missing)"} != v${version}`);
  process.stdout.write(changelogSection(readFileSync(path.join(root, "CHANGELOG.md"), "utf8"), version));
}
