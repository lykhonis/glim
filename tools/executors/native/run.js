const { spawn, spawnSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

function xcrun(args, opts = {}) {
  return spawnSync("xcrun", args, { encoding: "utf8", ...opts });
}

function timedOut(r) {
  return Boolean(r && r.error && (r.error.code === "ETIMEDOUT" || r.signal));
}

function simulatorWindowOrientation(udid) {
  const plist = `${process.env.HOME || ""}/Library/Preferences/com.apple.iphonesimulator.plist`;
  const r = spawnSync(
    "plutil",
    ["-extract", `DevicePreferences.${udid}.SimulatorWindowOrientation`, "raw", plist],
    { encoding: "utf8" },
  );
  if (r.status !== 0) {
    return "";
  }
  return (r.stdout || "").trim();
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

  if (device.state !== "Booted") {
    const boot = xcrun(["simctl", "boot", device.udid], { encoding: "utf8" });
    const err = (boot.stderr || boot.stdout || "").trim();
    if (boot.status !== 0 && !/current state:/i.test(err)) {
      return fail(`simctl boot failed: ${err || "unknown error"}`);
    }
  }

  spawnSync("open", ["-a", "Simulator"], { stdio: "ignore" });

  if (device.state !== "Booted") {
    const ready = xcrun(["simctl", "bootstatus", device.udid, "-b"], {
      stdio: "inherit",
      timeout: 180000,
    });
    if (ready.status !== 0 && !timedOut(ready)) {
      return fail(`simulator ${device.name} did not finish booting`);
    }
  }

  const sign = spawnSync("codesign", ["--force", "--sign", "-", "--timestamp=none", app], {
    encoding: "utf8",
  });
  if (sign.status !== 0) {
    const err = (sign.stderr || sign.stdout || "").trim();
    return fail(`codesign failed: ${err || "unknown error"}`);
  }

  console.log(`Installing ${app}`);
  let install = retry(3, 1, () =>
    xcrun(["simctl", "install", device.udid, app], { stdio: "inherit", timeout: 30000 }),
  );
  if (install.status !== 0) {
    xcrun(["simctl", "uninstall", device.udid, bundleId], { stdio: "ignore", timeout: 10000 });
    install = retry(3, 1, () =>
      xcrun(["simctl", "install", device.udid, app], { stdio: "inherit", timeout: 30000 }),
    );
  }
  if (install.status !== 0) {
    return fail(`simctl install failed for ${app}`);
  }

  console.log(`Launching ${bundleId} on ${device.name}`);
  const args = Array.isArray(options.args) ? options.args : [];
  const env = { ...process.env };
  const simOrient = simulatorWindowOrientation(device.udid);
  if (/landscape/i.test(simOrient)) {
    env.SIMCTL_CHILD_GLIM_SIM_ORIENTATION = "landscape";
  } else if (/portrait/i.test(simOrient)) {
    env.SIMCTL_CHILD_GLIM_SIM_ORIENTATION = "portrait";
  }
  if (simOrient) {
    console.log(`Simulator window: ${simOrient}`);
  }
  const launch = xcrun(
    ["simctl", "launch", "--terminate-running-process", device.udid, bundleId, ...args],
    { encoding: "utf8", timeout: 15000, env },
  );
  if (launch.status !== 0) {
    const err = (launch.stderr || launch.stdout || (launch.error && launch.error.message) || "").trim();
    return fail(`simctl launch failed: ${err || "unknown error"}`);
  }
  const launched = (launch.stdout || "").trim();
  if (launched) {
    console.log(launched);
  }
  return { success: true };
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

function androidSdkRoot() {
  const candidates = [
    process.env.ANDROID_HOME,
    process.env.ANDROID_SDK_ROOT,
    path.join(os.homedir(), "Library", "Android", "sdk"),
    path.join(os.homedir(), "Android", "Sdk"),
  ].filter(Boolean);
  for (const dir of candidates) {
    if (fs.existsSync(dir)) {
      return dir;
    }
  }
  return null;
}

function androidTool(sdk, rel) {
  if (!sdk) {
    return null;
  }
  const p = path.join(sdk, ...rel);
  if (fs.existsSync(p)) {
    return p;
  }
  if (process.platform === "win32" && fs.existsSync(`${p}.exe`)) {
    return `${p}.exe`;
  }
  return null;
}

function findAdb(sdk) {
  return androidTool(sdk, ["platform-tools", "adb"]) || which("adb");
}

function findEmulator(sdk) {
  return androidTool(sdk, ["emulator", "emulator"]) || which("emulator");
}

function matchesHint(text, hint) {
  if (!hint) {
    return true;
  }
  return String(text || "").toLowerCase().includes(hint);
}

function listAdbDevices(adbPath) {
  const r = spawnSync(adbPath, ["devices", "-l"], { encoding: "utf8" });
  if (r.status !== 0) {
    const err = ((r.stderr || r.stdout || "").trim());
    return { error: err || "adb devices failed" };
  }
  const devices = [];
  for (const line of (r.stdout || "").split("\n")) {
    const m = line.match(/^(\S+)\s+(device|offline|unauthorized|authorizing|no permissions)\s*(.*)$/);
    if (!m) {
      continue;
    }
    const serial = m[1];
    const rest = m[3] || "";
    const model = (rest.match(/model:(\S+)/) || [])[1] || "";
    const product = (rest.match(/product:(\S+)/) || [])[1] || "";
    devices.push({
      serial,
      state: m[2],
      model,
      product,
      avd: "",
      emulator: serial.startsWith("emulator-"),
      name: model || product || serial,
    });
  }
  return { devices };
}

function listAvds(emulatorPath) {
  const r = spawnSync(emulatorPath, ["-list-avds"], { encoding: "utf8" });
  if (r.status !== 0) {
    const err = ((r.stderr || r.stdout || "").trim());
    return { error: err || "emulator -list-avds failed" };
  }
  return {
    avds: (r.stdout || "")
      .split(/\r?\n/)
      .map((s) => s.trim())
      .filter(Boolean),
  };
}

function waitForAndroidDevice(adbPath, serial, timeoutMs, emulatorOnly, excludeSerials) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const listed = listAdbDevices(adbPath);
    if (!listed.error) {
      const ready = listed.devices.filter(
        (d) =>
          d.state === "device" &&
          (!emulatorOnly || d.emulator) &&
          (!excludeSerials || !excludeSerials.has(d.serial)),
      );
      const d = serial ? ready.find((x) => x.serial === serial) : ready[0];
      if (d) {
        const boot = spawnSync(adbPath, ["-s", d.serial, "shell", "getprop", "sys.boot_completed"], {
          encoding: "utf8",
        });
        if ((boot.stdout || "").trim() === "1") {
          return d;
        }
      }
    }
    spawnSync("sleep", ["1"]);
  }
  return null;
}

function fillAvdNames(adbPath, devices) {
  for (const d of devices) {
    if (!d.emulator) {
      continue;
    }
    const name = spawnSync(adbPath, ["-s", d.serial, "emu", "avd", "name"], {
      encoding: "utf8",
      timeout: 2000,
    });
    const avd = (name.stdout || "").split(/\r?\n/).map((s) => s.trim()).find(Boolean) || "";
    if (avd) {
      d.avd = avd;
      d.name = avd;
    }
  }
}

function pickAndroid(adbPath, emulatorPath, nameHint) {
  const hint = (nameHint || "").toLowerCase();
  const listed = listAdbDevices(adbPath);
  if (listed.error) {
    return { error: listed.error };
  }
  fillAvdNames(adbPath, listed.devices);

  const online = listed.devices.filter((d) => {
    if (d.state !== "device") {
      return false;
    }
    return matchesHint(`${d.serial} ${d.name} ${d.model} ${d.product} ${d.avd}`, hint);
  });
  online.sort((a, b) => {
    const aPhone = /pixel|phone|sdk_gphone/i.test(a.name) ? 1 : 0;
    const bPhone = /pixel|phone|sdk_gphone/i.test(b.name) ? 1 : 0;
    if (aPhone !== bPhone) {
      return bPhone - aPhone;
    }
    return String(a.name).localeCompare(String(b.name));
  });
  if (online.length > 0) {
    return { device: online[0] };
  }

  if (hint) {
    const serialMatch = listed.devices.find((d) => d.serial.toLowerCase() === hint);
    if (serialMatch && serialMatch.state !== "device") {
      return { device: serialMatch };
    }
  }

  if (!emulatorPath) {
    return {
      error:
        "No Android device connected, and the emulator binary was not found.\n" +
        "Connect a device with USB debugging, or install Android emulator:\n" +
        "  sdkmanager emulator",
    };
  }

  const avds = listAvds(emulatorPath);
  if (avds.error) {
    return { error: avds.error };
  }
  const candidates = avds.avds.filter((name) => matchesHint(name, hint));
  if (candidates.length === 0) {
    return {
      error:
        "No Android device or emulator." +
        (hint ? ` None matched "${nameHint}".` : "") +
        "\nConnect a device, or create an AVD in Android Studio (Device Manager).",
    };
  }
  candidates.sort((a, b) => {
    const score = (n) => (/pixel|phone/i.test(n) && !/tv|wear|automotive/i.test(n) ? 1 : 0);
    const d = score(b) - score(a);
    return d !== 0 ? d : a.localeCompare(b);
  });
  return { avd: candidates[0] };
}

function bootAvd(emulatorPath, sdk, avd) {
  const child = spawn(emulatorPath, ["-avd", avd], {
    detached: true,
    stdio: "ignore",
    env: {
      ...process.env,
      ...(sdk ? { ANDROID_HOME: sdk, ANDROID_SDK_ROOT: sdk } : {}),
    },
  });
  child.unref();
}

function runOnAndroid(options, apk) {
  const pkg = options.packageName;
  if (!pkg) {
    return fail("@glim/native:run with android requires options.packageName");
  }

  const sdk = androidSdkRoot();
  const adbPath = findAdb(sdk);
  if (!adbPath) {
    return fail(
      "adb not found. Set ANDROID_HOME, or install platform-tools:\n" +
        "  sdkmanager platform-tools",
    );
  }
  spawnSync(adbPath, ["start-server"], { stdio: "ignore" });

  const emulatorPath = findEmulator(sdk);
  const picked = pickAndroid(adbPath, emulatorPath, options.device);
  if (picked.error) {
    return fail(picked.error);
  }

  let serial;
  if (picked.avd) {
    const listed = listAdbDevices(adbPath);
    const known = new Set((listed.devices || []).map((d) => d.serial));
    console.log(`Emulator: ${picked.avd}`);
    bootAvd(emulatorPath, sdk, picked.avd);
    const ready = waitForAndroidDevice(adbPath, null, 180000, true, known);
    if (!ready) {
      return fail(`emulator ${picked.avd} did not boot`);
    }
    serial = ready.serial;
    console.log(`Emulator: ${picked.avd} (${serial}, booted)`);
  } else {
    console.log(`Device: ${picked.device.name} (${picked.device.serial}, ${picked.device.state})`);
    const ready = waitForAndroidDevice(adbPath, picked.device.serial, 180000);
    if (!ready) {
      return fail(`Android device ${picked.device.serial} did not become ready`);
    }
    serial = ready.serial;
  }

  console.log(`Installing ${apk}`);
  const install = retry(5, 2, () =>
    spawnSync(adbPath, ["-s", serial, "install", "-r", "-t", apk], { stdio: "inherit" }),
  );
  if (install.status !== 0) {
    return fail(`adb install failed for ${apk}`);
  }

  const component = `${pkg}/android.app.NativeActivity`;
  spawnSync(adbPath, ["-s", serial, "shell", "am", "force-stop", pkg], { stdio: "ignore" });
  console.log(`Launching ${component} on ${serial}`);
  const args = Array.isArray(options.args) ? options.args : [];
  const launch = spawnSync(
    adbPath,
    ["-s", serial, "shell", "am", "start", "-n", component, ...args],
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
