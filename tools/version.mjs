// @ts-check
import { readFileSync } from "node:fs";
import path from "node:path";

/** @param {string} root */
export function glimVersion(root) {
  const version = /project\(glim VERSION (\S+)/.exec(readFileSync(path.join(root, "CMakeLists.txt"), "utf8"))?.[1];
  if (!version) throw new Error("no project(glim VERSION ...) in CMakeLists.txt");
  return version;
}

/** @param {string} root */
export function requireMatchingVersion(root) {
  const version = glimVersion(root);
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error(`glim version ${version} is not major.minor.patch`);
  const pkg = JSON.parse(readFileSync(path.join(root, "packages", "glim-web", "package.json"), "utf8"));
  if (pkg.version !== version) throw new Error(`@glim/web ${pkg.version} != ${version}`);
  return version;
}
