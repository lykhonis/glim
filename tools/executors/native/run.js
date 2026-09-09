const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

exports.default = async function runExecutor(options, context) {
  const root = context.root;
  const preset = options.preset || "macos-metal-debug";
  if (!options.binary) {
    console.error("@glim/native:run requires options.binary");
    return { success: false };
  }
  const exe = path.isAbsolute(options.binary)
    ? options.binary
    : path.join(root, "build", preset, options.binary);
  if (!fs.existsSync(exe)) {
    console.error(`missing binary: ${exe} (build the example first)`);
    return { success: false };
  }
  const args = Array.isArray(options.args) ? options.args : [];
  const r = spawnSync(exe, args, { cwd: root, stdio: "inherit" });
  if (r.error) {
    console.error(r.error.message);
    return { success: false };
  }
  return { success: r.status === 0 };
};
