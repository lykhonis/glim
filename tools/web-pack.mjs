// @ts-check
import { execFileSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { requireMatchingVersion } from "./version.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const work = path.join(root, "build", "web-pack");
const stage = path.join(work, "stage");
const tarball = path.join(work, "glim-web.tgz");
const built = path.join(root, "build", "web-webgpu-release", "examples", "web");

/** @param {string} cmd @param {string[]} args @param {string} [cwd] */
function run(cmd, args, cwd = root) {
  console.log(`$ ${cmd} ${args.join(" ")}`);
  execFileSync(cmd, args, { cwd, stdio: "inherit" });
}

const version = requireMatchingVersion(root);
run("cmake", ["--preset", "web-webgpu-release"]);
run("cmake", ["--build", path.join(root, "build", "web-webgpu-release"), "--target", "glim-web"]);

const moduleJs = path.join(built, "glim-app.js");
const moduleWasm = path.join(built, "glim-app.wasm");
if (!existsSync(moduleJs) || !existsSync(moduleWasm)) {
  throw new Error(`missing ${moduleJs} or ${moduleWasm}`);
}

rmSync(stage, { recursive: true, force: true });
mkdirSync(path.join(stage, "dist"), { recursive: true });
cpSync(path.join(root, "packages", "glim-web", "js"), path.join(stage, "js"), { recursive: true });
cpSync(path.join(root, "packages", "glim-web", "README.md"), path.join(stage, "README.md"));
cpSync(path.join(root, "LICENSE"), path.join(stage, "LICENSE"));
cpSync(moduleJs, path.join(stage, "dist", "glim-app.js"));
cpSync(moduleWasm, path.join(stage, "dist", "glim-app.wasm"));

const pkg = JSON.parse(readFileSync(path.join(root, "packages", "glim-web", "package.json"), "utf8"));
writeFileSync(path.join(stage, "package.json"), `${JSON.stringify({
  ...pkg,
  version,
  exports: {
    ".": { types: "./js/glim.d.ts", default: "./js/glim.js" },
    "./dist/*": "./dist/*",
  },
  files: ["js/", "dist/", "README.md", "LICENSE"],
  repository: { type: "git", url: "git+https://github.com/lykhonis/glim.git" },
}, null, 2)}\n`);

const packed = path.join(work, `glim-web-${version}.tgz`);
rmSync(packed, { force: true });
rmSync(tarball, { force: true });
run("npm", ["pack", "--pack-destination", work], stage);
cpSync(packed, tarball);
run("npm", ["publish", "--dry-run", "--access", "public"], stage);

const listing = execFileSync("tar", ["-tzf", tarball], { encoding: "utf8" });
for (const file of ["package.json", "README.md", "LICENSE", "js/glim.js", "js/glim.d.ts", "dist/glim-app.js", "dist/glim-app.wasm"]) {
  if (!listing.includes(file)) throw new Error(`${tarball} missing ${file}`);
}
console.log(`glim web pack ok (${version})`);
