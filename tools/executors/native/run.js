const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

function xcrun(args, opts = {}) {
  return spawnSync("xcrun", args, { encoding: "utf8", ...opts });
}

function fail(message) {
  console.error(message);
  return { success: false };
}

function resolveAppBundle(exe) {
  if (exe.endsWith(".app") && fs.existsSync(exe)) {
    return exe;
  }
  const dir = path.dirname(exe);
  if (dir.endsWith(".app") && fs.existsSync(dir)) {
    return dir;
  }
  if (fs.existsSync(`${exe}.app`)) {
    return `${exe}.app`;
  }
  return null;
}

function runtimeKind(runtime) {
  if (/tvOS/i.test(runtime)) {
    return "tvos";
  }
  if (/iOS/i.test(runtime)) {
    return "ios";
  }
  return null;
}

function runtimeVersion(runtime) {
  const nums = String(runtime).match(/\d+/g);
  if (!nums) {
    return 0;
  }
  return nums.reduce((acc, n) => acc * 1000 + Number(n), 0);
}

function listDevices() {
  const listed = xcrun(["simctl", "list", "devices", "-j"]);
  if (listed.status !== 0) {
    const err = ((listed.stderr || (listed.error && listed.error.message) || "").trim());
    return { error: err || "xcrun simctl list failed" };
  }
  try {
    return { data: JSON.parse(listed.stdout) };
  } catch (err) {
    return { error: `could not parse simctl JSON: ${err.message}` };
  }
}

function findDevice(data, udid) {
  for (const devices of Object.values(data.devices || {})) {
    for (const device of devices) {
      if (device.udid === udid) {
        return device;
      }
    }
  }
  return null;
}

function waitUntilBooted(udid, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const listed = listDevices();
    if (!listed.error) {
      const device = findDevice(listed.data, udid);
      if (device && device.state === "Booted") {
        return true;
      }
      if (device && (device.state === "Shutdown" || device.state === "Shutting Down")) {
        xcrun(["simctl", "boot", udid], { stdio: "ignore" });
      }
    }
    spawnSync("sleep", ["1"]);
  }
  return false;
}

function retry(times, delaySec, fn) {
  let last = { status: 1 };
  for (let i = 0; i < times; i++) {
    last = fn();
    if (last.status === 0) {
      return last;
    }
    if (i + 1 < times) {
      spawnSync("sleep", [String(delaySec)]);
    }
  }
  return last;
}

function pickSimulator(kind, nameHint) {
  const listed = listDevices();
  if (listed.error) {
    return { error: listed.error };
  }

  const hint = (nameHint || "").toLowerCase();
  const candidates = [];
  for (const [runtime, devices] of Object.entries(listed.data.devices || {})) {
    if (runtimeKind(runtime) !== kind) {
      continue;
    }
    for (const device of devices) {
      if (device.isAvailable === false) {
        continue;
      }
      if (hint && !String(device.name || "").toLowerCase().includes(hint)) {
        continue;
      }
      candidates.push({ ...device, runtime });
    }
  }
  if (candidates.length === 0) {
    const platform = kind === "tvos" ? "tvOS" : "iOS";
    return {
      error:
        `No available ${platform} simulator.` +
        (hint ? ` None matched "${nameHint}".` : "") +
        `\nInstall a ${platform} runtime (Xcode > Settings > Platforms) or:\n` +
        `  xcodebuild -downloadPlatform ${platform}`,
    };
  }

  const preferredName = kind === "tvos" ? /apple tv/i : /iphone/i;
  candidates.sort((a, b) => {
    const aBoot = a.state === "Booted" ? 1 : 0;
    const bBoot = b.state === "Booted" ? 1 : 0;
    if (aBoot !== bBoot) {
      return bBoot - aBoot;
    }
    const aPref = preferredName.test(a.name) ? 1 : 0;
    const bPref = preferredName.test(b.name) ? 1 : 0;
    if (aPref !== bPref) {
      return bPref - aPref;
    }
    const ver = runtimeVersion(b.runtime) - runtimeVersion(a.runtime);
    if (ver !== 0) {
      return ver;
    }
    return String(a.name).localeCompare(String(b.name));
  });
  return { device: candidates[0] };
}

function runOnSimulator(options, app) {
  const kind = options.simulator;
  const bundleId = options.bundleId;
  if (!bundleId) {
    return fail("@glim/native:run with simulator requires options.bundleId");
  }

  const picked = pickSimulator(kind, options.device);
  if (picked.error) {
    return fail(picked.error);
  }
  const device = picked.device;
  console.log(`Simulator: ${device.name} (${device.runtime}, ${device.state})`);

  spawnSync("open", ["-a", "Simulator", "--args", "-CurrentDeviceUDID", device.udid], {
    stdio: "ignore",
  });

  const boot = xcrun(["simctl", "bootstatus", device.udid, "-b"], { stdio: "inherit" });
  if (boot.status !== 0) {
    xcrun(["simctl", "boot", device.udid], { stdio: "inherit" });
  }
  if (!waitUntilBooted(device.udid, 180000)) {
    return fail(`simulator ${device.name} did not reach Booted`);
  }

  const sign = spawnSync("codesign", ["--force", "--sign", "-", "--timestamp=none", app], {
    encoding: "utf8",
  });
  if (sign.status !== 0) {
    const err = (sign.stderr || sign.stdout || "").trim();
    return fail(`codesign failed: ${err || "unknown error"}`);
  }

  xcrun(["simctl", "terminate", device.udid, bundleId], { stdio: "ignore" });
  xcrun(["simctl", "uninstall", device.udid, bundleId], { stdio: "ignore" });

  const install = retry(5, 2, () =>
    xcrun(["simctl", "install", device.udid, app], { stdio: "inherit" }),
  );
  if (install.status !== 0) {
    return fail(`simctl install failed for ${app}`);
  }

  console.log(`Launching ${bundleId} on ${device.name}`);
  const args = Array.isArray(options.args) ? options.args : [];
  const launch = xcrun(
    ["simctl", "launch", "--console", device.udid, bundleId, ...args],
    { stdio: "inherit" },
  );
  if (launch.error) {
    return fail(launch.error.message);
  }
  return { success: launch.status === 0 };
}

function runOnAndroid(options, apk) {
  const pkg = options.packageName;
  if (!pkg) {
    return fail("@glim/native:run with android requires options.packageName");
  }
  const serial = options.device;
  const adb = serial ? ["adb", "-s", serial] : ["adb"];
  console.log(`Installing ${apk}`);
  const install = spawnSync(adb[0], [...adb.slice(1), "install", "-r", apk], { stdio: "inherit" });
  if (install.status !== 0) {
    return fail("adb install failed (is a device or emulator connected?)");
  }
  const component = `${pkg}/android.app.NativeActivity`;
  console.log(`Launching ${component}`);
  const launch = spawnSync(
    adb[0],
    [...adb.slice(1), "shell", "am", "start", "-n", component],
    { stdio: "inherit" },
  );
  if (launch.error) {
    return fail(launch.error.message);
  }
  return { success: launch.status === 0 };
}

function resolvePreset(preset) {
  if (process.platform === "linux") {
    if (!preset || /^macos-/.test(preset)) {
      return /release/i.test(preset || "") ? "linux-vulkan-release" : "linux-vulkan-debug";
    }
  }
  return preset || "macos-metal-debug";
}

exports.default = async function runExecutor(options, context) {
  const root = context.root;
  const preset = resolvePreset(options.preset);
  if (!options.binary) {
    return fail("@glim/native:run requires options.binary");
  }
  const exe = path.isAbsolute(options.binary)
    ? options.binary
    : path.join(root, "build", preset, options.binary);

  if (options.simulator) {
    const app = resolveAppBundle(exe);
    if (!app) {
      return fail(`missing app bundle: ${exe} (build the example first)`);
    }
    return runOnSimulator(options, app);
  }

  if (options.android) {
    if (!fs.existsSync(exe)) {
      return fail(`missing APK: ${exe} (build the example first; needs Android SDK build-tools)`);
    }
    return runOnAndroid(options, exe);
  }

  if (!fs.existsSync(exe)) {
    return fail(`missing binary: ${exe} (build the example first)`);
  }
  const args = Array.isArray(options.args) ? options.args : [];
  const r = spawnSync(exe, args, { cwd: root, stdio: "inherit" });
  if (r.error) {
    return fail(r.error.message);
  }
  return { success: r.status === 0 };
};
