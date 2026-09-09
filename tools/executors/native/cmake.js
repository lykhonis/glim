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

function withBuildLock(lockFile, fn) {
  fs.mkdirSync(path.dirname(lockFile), { recursive: true });
  const deadline = Date.now() + 120000;
  let fd;
  for (;;) {
    try {
      fd = fs.openSync(lockFile, "wx");
      break;
    } catch (err) {
      if (err.code !== "EEXIST") {
        throw err;
      }
      if (Date.now() > deadline) {
        throw new Error(`timed out waiting for ${lockFile}`);
      }
      spawnSync("sleep", ["0.2"]);
    }
  }
  try {
    fs.writeSync(fd, String(process.pid));
    return fn();
  } finally {
    fs.closeSync(fd);
    try {
      fs.unlinkSync(lockFile);
    } catch {
      // ignore
    }
  }
}

exports.default = async function cmakeExecutor(options, context) {
  const root = context.root;
  const preset = options.preset;
  const target = options.target;
  const dist = path.join(root, "dist", context.projectName);
  const buildDir = preset
    ? path.join(root, "build", preset)
    : options.buildDir || path.join(root, "build", "default");
  const lockFile = path.join(path.dirname(buildDir), `.${path.basename(buildDir)}.cmake.lock`);

  const compilerId = path.join(root, "tools/scripts/compiler-id.sh");
  if (fs.existsSync(compilerId)) {
    spawnSync("bash", [compilerId], { cwd: root, stdio: "inherit" });
  }

  return withBuildLock(lockFile, () => {
    let ok;
    if (preset) {
      ok = run("cmake", ["--preset", preset], root);
      if (!ok) return { success: false };
      ok = run("cmake", ["--build", "--preset", preset, "--target", target], root);
      if (!ok) return { success: false };
      if (options.install && fs.existsSync(path.join(buildDir, "cmake_install.cmake"))) {
        fs.mkdirSync(dist, { recursive: true });
        ok = run("cmake", ["--install", buildDir, "--prefix", dist], root);
        if (!ok) return { success: false };
      }
    } else {
      fs.mkdirSync(buildDir, { recursive: true });
      ok = run("cmake", ["-S", root, "-B", buildDir, "-G", "Ninja"], root);
      if (!ok) return { success: false };
      ok = run("cmake", ["--build", buildDir, "--target", target], root);
      if (!ok) return { success: false };
    }

    if (options.ctestRegex) {
      ok = run(
        "ctest",
        ["--test-dir", buildDir, "--output-on-failure", "-R", options.ctestRegex],
        root,
      );
      if (!ok) return { success: false };
    }

    return { success: true };
  });
};
