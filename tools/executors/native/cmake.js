const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

function run(cmd, args, cwd) {
  const r = spawnSync(cmd, args, { cwd, stdio: "inherit" });
  if (r.error) {
    throw r.error;
  }
  if (r.status !== 0) {
    return false;
  }
  return true;
}

exports.default = async function cmakeExecutor(options, context) {
  const root = context.root;
  const preset = options.preset;
  const target = options.target;
  const dist = path.join(root, "dist", context.projectName);

  const compilerId = path.join(root, "tools/scripts/compiler-id.sh");
  if (fs.existsSync(compilerId)) {
    spawnSync("bash", [compilerId], { cwd: root, stdio: "inherit" });
  }

  let ok;
  if (preset) {
    ok = run("cmake", ["--preset", preset], root);
    if (!ok) return { success: false };
    ok = run("cmake", ["--build", "--preset", preset, "--target", target], root);
    if (!ok) return { success: false };
    const buildDir = path.join(root, "build", preset);
    if (fs.existsSync(path.join(buildDir, "cmake_install.cmake"))) {
      fs.mkdirSync(dist, { recursive: true });
      ok = run("cmake", ["--install", buildDir, "--prefix", dist], root);
      if (!ok) return { success: false };
    }
  } else {
    const buildDir = options.buildDir || path.join(root, "build", "default");
    fs.mkdirSync(buildDir, { recursive: true });
    ok = run("cmake", ["-S", root, "-B", buildDir, "-G", "Ninja"], root);
    if (!ok) return { success: false };
    ok = run("cmake", ["--build", buildDir, "--target", target], root);
    if (!ok) return { success: false };
  }

  return { success: true };
};
