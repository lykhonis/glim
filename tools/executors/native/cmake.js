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

function resolvePreset(preset) {
  if (process.platform === "linux") {
    if (!preset || /^macos-/.test(preset)) {
      return /release/i.test(preset || "") ? "linux-vulkan-release" : "linux-vulkan-debug";
    }
  }
  return preset || "macos-metal-debug";
}

function which(cmd) {
  const bin = process.platform === "win32" ? "where" : "which";
  const r = spawnSync(bin, [cmd], { encoding: "utf8" });
  if (r.status !== 0) {
    return null;
  }
  return (r.stdout || "")
    .split(/\r?\n/)
    .map((s) => s.trim())
    .find(Boolean) || null;
}

function emscriptenToolchain() {
  if (process.env.EMSDK) {
    return { file: null };
  }
  if (!which("em-config")) {
    return {
      error:
        `EMSDK is unset and em-config was not found on PATH.\n` +
        "Install emsdk (https://emscripten.org/docs/getting_started/downloads.html), then\n" +
        "in the same shell run:\n" +
        '  source "$EMSDK/emsdk_env.sh"',
    };
  }
  const r = spawnSync("em-config", ["EMSCRIPTEN_ROOT"], { encoding: "utf8" });
  const toolchain = r.status === 0 && `${(r.stdout || "").trim()}/cmake/Modules/Platform/Emscripten.cmake`;
  if (!toolchain || !fs.existsSync(toolchain)) {
    return { error: "em-config did not resolve to an Emscripten.cmake toolchain file." };
  }
  return { file: toolchain };
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
      try {
        const pid = parseInt(fs.readFileSync(lockFile, "utf8"), 10);
        if (Number.isFinite(pid)) {
          process.kill(pid, 0);
        } else {
          throw new Error("no pid");
        }
      } catch {
        try {
          fs.unlinkSync(lockFile);
        } catch {}
        continue;
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
  const preset = resolvePreset(options.preset);
  const target = options.target;
  const dist = path.join(root, "dist", context.projectName);
  const buildDir = preset
    ? path.join(root, "build", preset)
    : options.buildDir || path.join(root, "build", "default");
  const lockFile = path.join(path.dirname(buildDir), `.${path.basename(buildDir)}.cmake.lock`);
  const extraDefines = [];

  const compilerId = path.join(root, "tools/scripts/compiler-id.sh");
  if (fs.existsSync(compilerId)) {
    spawnSync("bash", [compilerId], { cwd: root, stdio: "inherit" });
  }

  if (/^web-/.test(preset || "")) {
    const toolchain = emscriptenToolchain();
    if (toolchain.error) {
      console.error(toolchain.error);
      return { success: false };
    }
    if (toolchain.file) {
      extraDefines.push(`-DCMAKE_TOOLCHAIN_FILE=${toolchain.file}`);
    }
  }

  return withBuildLock(lockFile, () => {
    let ok;
    if (preset) {
      ok = run("cmake", ["--preset", preset, ...extraDefines], root);
      if (!ok) return { success: false };
      ok = run("cmake", ["--build", "--preset", preset, "--target", target], root);
      if (!ok) return { success: false };
      if (options.install && fs.existsSync(path.join(buildDir, "cmake_install.cmake"))) {
        fs.mkdirSync(dist, { recursive: true });
        ok = run(
          "cmake",
          ["--install", buildDir, "--prefix", dist, "--component", target],
          root,
        );
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
